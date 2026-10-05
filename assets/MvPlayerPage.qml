// MvPlayerPage - MV 播放页（由 NavigationPane push 进来）
//
// 视频输出用官方 VideoPlayerSample 的做法：
//   ForeignWindowControl（视频窗口）+ MediaPlayer.videoOutput / windowId
//   （示例是旧版 ForeignWindow / VideoOutputPrimary，10.3 用
//    ForeignWindowControl / VideoOutput.PrimaryDisplay）

import bb.cascades 1.4
import bb.multimedia 1.0

Page {
    Menu.definition: MenuDefinition {
        actions: [
            ActionItem {
                title: "正在播放"
                imageSource: "asset:///icons/ic_play_on.png"
                onTriggered: {
                    music.requestOpenNowPlaying()
                }
            }
        ]
    }
    actions: [
        // ★ 播放/暂停（从「正在播放页」拿过来的同款逻辑，这里对象换成 mvPlayer）
        ActionItem {
            // 文字/图标随状态切换：播放中显示「暂停」+暂停图标，否则「播放」+播放图标
            title: mvPlayer.mediaState == MediaState.Started ? qsTr("暂停") : qsTr("播放")
            imageSource: mvPlayer.mediaState == MediaState.Started
                         ? "asset:///icons/ic_pause.png"
                         : "asset:///icons/ic_play.png"
            ActionBar.placement: ActionBarPlacement.Signature
            onTriggered: {
                if (mvPlayer.mediaState == MediaState.Started)
                    mvPlayer.pause()
                /*
                 * ★ 走 tryPlay()，不要直接 mvPlayer.play()：
                 *   mvUrlVersion > 0 只说明"请求过"，不代表 sourceUrl 已经设上；
                 *   直接 play() 会拿【空 url】去播，就是日志里那句
                 *   "attachInput: Unable to attach source input, e=SourceUnavailable"。
                 *   tryPlay() 会先补齐 sourceUrl 再播。
                 */
                else
                    mvPage.tryPlay()
            }
        },
        // ★ 用 InvokeActionItem 把 mvUrl 交给系统浏览器。uri 绑到 music.mvUrl：
        //   地址带时效性签名、每次进来都可能不同，绑属性比写死稳。
        InvokeActionItem {
            title: qsTr("浏览器打开")
            imageSource: "asset:///icons/ic_browser.png"
            ActionBar.placement: ActionBarPlacement.OnBar
            query {
                // ★ 属性名对着 NDK 头核对过：动作是 invokeActionId、目标是
                //   invokeTargetId（不是 invokeAction / target，写错加载即崩）
                invokeActionId: "bb.action.OPEN"
                uri: music.mvUrl
                invokeTargetId: "sys.browser"
            }
        },
        /*
         * ★ 交给系统播放器：用 invocation 框架把 mvUrl 丢给系统，
         *   【不写死 invokeTargetId】—— 让系统按 URI 类型（.mp4）自己挑
         *   （通常是内置视频播放器）。写死 target 的话一旦 id 不对就没反应。
         *
         *   为什么需要这条：本页的 ForeignWindowControl 在部分机型/环境
         *   始终绑不上窗口（boundToWindow 恒 false），内联播放用不了，
         *   这时用系统播放器兜底——至少能把 MV 看/听完。
         */
//        InvokeActionItem {
//            title: qsTr("系统播放器打开")
//            imageSource: "asset:///icons/ic_play.png"
//            ActionBar.placement: ActionBarPlacement.OnBar
//            query {
//                invokeActionId: "bb.action.OPEN"
//                uri: music.mvUrl
//            }
//        },
        ActionItem {
            title: qsTr("复制链接")
            imageSource: "asset:///icons/ic_copy.png"
            ActionBar.placement: ActionBarPlacement.Default
            onTriggered: {
                music.copyToClipboard(music.mvUrl)
                music.notifyError(qsTr("已复制链接"))
            }
        }
    ]
    id: mvPage

    objectName: "mvPlayerPage"

    // ★ 自动开播的「就绪」开关：视频窗口已绑定 且 地址已回来。
    //   用这个派生属性（而非专门的信号连接类型 —— 本项目 QML 引擎不认那个类型）
    //   统一监听两个异步事件；任一就绪都会让绑定重算，翻成 true 即开播。
    property bool mvReady: mvSurface.boundToWindow && music.mvUrl.length > 0
    onMvReadyChanged: {
        if (mvReady)
            mvPage.tryPlay()
    }

    /*
     * ★ 再补一道：地址是带时效性签名的，每次进来都是新的。
     *   只靠 mvReady 不够——从「一个非空地址」换成「另一个非空地址」时
     *   mvUrl.length 始终 > 0，mvReady 不会翻转，onMvReadyChanged 就不触发，
     *   表现就是换了个 MV 却不开播。这里直接盯版本号，一变就再试一次。
     */
    property int mvUrlVer: music.mvUrlVersion
    onMvUrlVerChanged: {
        mvPage.tryPlay()
    }

    titleBar: TitleBar {
        title: music.mvName
    }

    Container {
        background: Color.Black
        layout: StackLayout {
            orientation: LayoutOrientation.TopToBottom
        }

        // ---- 视频窗口 ----
        Container {
            background: Color.Black
            horizontalAlignment: HorizontalAlignment.Fill
            preferredHeight: 420
            layout: DockLayout {
            }
            ForeignWindowControl {
                id: mvSurface
                windowId: "mvSurfaceWindow"
                /*
                 * ★★ 原来这里写的是 `visible: boundToWindow` —— 这是个死循环：
                 *   绑定了才可见，但不可见就永远不会被拿去绑定。
                 *   真机实测 boundToWindow 一直是 false（tryPlay 被调了 7 次
                 *   全是 bound=false），MV 因此从来没播起来过。
                 *   ★ 保持常驻可见（默认就是 true），让它有机会完成绑定；
                 *     父容器是纯黑底，没绑上时也不会露出难看的东西。
                 */
                updatedProperties: WindowProperty.Size
                                       | WindowProperty.Position
                                       | WindowProperty.Visible
                horizontalAlignment: HorizontalAlignment.Fill
                verticalAlignment: VerticalAlignment.Fill

                // ★ 视频窗口必须先 boundToWindow 之后，才能把播放器接上去并播放
                //   （官方 VideoPlayerSample 同款做法）。在 onCreationCompleted
                //   里就 sourceUrl+play 时窗口还没绑好，结果就是黑屏。
                onBoundToWindowChanged: {
                    // ★ 窗口绑定时先别急着播：地址可能还没回来（#7 常晚于页面推入）。
                    //   交给 tryPlay() 统一判断「窗口已绑定 + 地址就绪」两个前提。
                    mvPage.tryPlay()
                }
            }
        }

        // ---- 视频窗口下方：分段「简介 / 评论」----
        SegmentedControl {
            id: mvMode
            // 切到「评论」才去拉，避免进页面就多发一次请求
            onSelectedValueChanged: {
                if (selectedValue == "comments")
                    music.requestMvComments()
            }
            Option {
                text: qsTr("简介")
                value: "intro"
                selected: true
            }
            Option {
                text: qsTr("评论")
                value: "comments"
            }
        }

        // ---- 简介 ----
        Container {
            visible: mvMode.selectedValue == "intro"
            leftPadding: 16
            rightPadding: 16
            topPadding: 12
            layout: StackLayout {
                orientation: LayoutOrientation.TopToBottom
            }
            Label {
                text: music.mvName
                textStyle {
                    base: SystemDefaults.TextStyles.TitleText
                }
            }
            Label {
                text: music.mvArtist
                textStyle {
                    base: SystemDefaults.TextStyles.SubtitleText
                    color: ui.palette.textOnPlain
                }
            }
        }

        // ---- 评论（复用歌曲评论那套模型 / 解析）----
        Container {
            visible: mvMode.selectedValue == "comments"
            layoutProperties: StackLayoutProperties {
                spaceQuota: 1
            }
            // 加载中
            Container {
                visible: music.loading
                leftPadding: 16
                layout: StackLayout {
                    orientation: LayoutOrientation.LeftToRight
                }
                ActivityIndicator {
                    running: true
                    preferredWidth: ui.sdu(6)
                    preferredHeight: ui.sdu(6)
                    verticalAlignment: VerticalAlignment.Center
                }
                Label {
                    text: qsTr("正在加载评论…")
                    verticalAlignment: VerticalAlignment.Center
                    textStyle {
                        base: SystemDefaults.TextStyles.SmallText
                    }
                }
            }
            // 空状态
            Label {
                visible: (!music.loading) && music.commentsVersion > 0
                         && music.commentCount() == 0
                // ★ Label 不支持 padding（会刷 "Padding is not supported"）→ 用 margin
                leftMargin: 16
                rightMargin: 16
                topMargin: 16
                text: qsTr("还没有评论")
                textStyle {
                    base: SystemDefaults.TextStyles.BodyText
                    color: ui.palette.textOnPlain
                }
            }
            ListView {
                dataModel: music.comments
                property variant controller: music
                listItemComponents: [
                    ListItemComponent {
                        type: ""
                        Container {
                            leftPadding: 16
                            rightPadding: 16
                            topPadding: 8
                            bottomPadding: 8
                            layout: StackLayout {
                                orientation: LayoutOrientation.TopToBottom
                            }
                            Label {
                                text: ListItemData.userName
                                textStyle {
                                    base: SystemDefaults.TextStyles.SubtitleText
                                    color: ui.palette.primary
                                }
                            }
                            Label {
                                text: ListItemData.content
                                multiline: true
                                textStyle {
                                    base: SystemDefaults.TextStyles.BodyText
                                }
                            }
                            Label {
                                // ★ Label 不支持 padding（真机日志会刷
                                //   "Padding is not supported for this control"），用 margin 代替
                                text: ListItemData.timeText
                                topMargin: 4
                                textStyle {
                                    base: SystemDefaults.TextStyles.SmallText
                                    color: ui.palette.textOnPlain
                                }
                            }
                        }
                    }
                ]
            }
        }
    }

    /*!
     * 真正接上播放器并开播的统一入口。
     * ★ 两个前提缺一不可：
     *   1) 视频窗口已绑定（ForeignWindowControl.boundToWindow）—— 没绑好就 play
     *      会黑屏（官方 VideoPlayerSample 的坑）；
     *   2) music.mvUrl 已就绪 —— 地址是带时效签名的，常在页面推入之后才回来，
     *      早一步 play 会得到 SourceUnavailable（url 为空）。
     * 窗口绑定 / 地址就绪 两个事件谁先到都不怕：先到的那次 tryPlay 因缺另一个
     * 前提直接返回，等另一个到了再触发一次就齐了。
     */
    function tryPlay() {
        // ★ 诊断：把实际要播的地址打出来（开「输出控制台日志」可见）
        music.trace("mvPage.tryPlay: bound=" + mvSurface.boundToWindow
                    + " v=" + music.mvUrlVersion
                    + " url=[" + music.mvUrl + "]")
        if (!mvSurface.boundToWindow || music.mvUrl.length === 0)
            return
        if (mvPlayer.sourceUrl !== music.mvUrl) {
            mvPlayer.sourceUrl = music.mvUrl
            mvPlayer.play()
        } else if (mvPlayer.mediaState !== MediaState.Started) {
            mvPlayer.play()
        }
    }

    attachedObjects: [
        MediaPlayer {
            id: mvPlayer
            videoOutput: VideoOutput.PrimaryDisplay
            /*
             * ★ 窗口【绑定之前】不要把 windowId 交给播放器。
             *   ForeignWindowControl 得先自己创建这个窗口；播放器抢先占用同名
             *   windowId 有可能让它一直绑不上（boundToWindow 恒为 false）。
             *   等绑定成功再把 windowId 接上去。
             */
            windowId: mvSurface.boundToWindow ? mvSurface.windowId : ""
        }
    ]

    /*
     * ⚠ 别在这里写 Component.onDestruction —— Cascades 没有 Component
     *   这个附加对象，写了整个 QML 文件加载失败（真机日志：
     *   "Non-existent attached object"），页面根本推不出来。
     *
     * 停止播放不需要手动处理：页面被 pop 并 destroy 时，里面的
     * MediaPlayer 一起析构，播放自然停止。
     */
}
