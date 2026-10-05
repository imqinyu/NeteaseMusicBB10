// NowPlayingBar - 顶部「正在播放」迷你条
//
// ★ 结构照官方 Music ui 的 NowPlayingMiniControl 的列表项：
//     [ 11du 正方形封面 ] [ 标题(主色) / 艺术家(次要色) ] ...... [ 11du 播放键 ]
//   整行高 SCREEN_TITLE_HEIGHT = 11du；左右 CONTROL_PADDING = 2du；
//   底部一条 Divider。
//
// ★ 数据用【直接属性表达式】，不能包函数；布局用 DockLayout（左 Fill 占满、
//   右播放键 dock 右侧），不要用 spaceQuota（pop 塌缩）或 L-R StackLayout 的 Fill
//   （不撑主轴）。

import bb.cascades 1.4
import bb.multimedia 1.0

Container {
    id: npBar

    signal openPlayer()

    /*
     * 是否真的有在播放的曲目。
     * ★ 不能只看 currentTrackVersion > 0：空 map 在 JS 里是真值，
     *   列表被清时会 currentTrack={} 却 version>0，判成"有曲目"→ 空白。
     *   必须要求 name 有值。
     */
    property bool hasTrack: music.currentTrackVersion > 0
                            && music.currentTrack
                            && music.currentTrack.name !== undefined
                            && music.currentTrack.name.length > 0

    /*
     * 调试：把迷你条当前状态打到控制台。
     * 每次曲目变化/创建时 trace 一行，方便定位"内容消失"。
     */
    property int trackVersion: music.currentTrackVersion
    onTrackVersionChanged: {
        music.trace("minibar: v=" + trackVersion
                    + " hasTrack=" + (npBar.hasTrack ? 1 : 0)
                    + " name=" + (npBar.hasTrack ? music.currentTrack.name : "-"))
    }
    onCreationCompleted: {
        music.trace("minibar: created, v=" + music.currentTrackVersion
                    + " hasTrack=" + (npBar.hasTrack ? 1 : 0))
    }

    /*
     * ★★ 整条的高度必须【钉死 = 内容行的高度】。
     *
     * 原来只给 minHeight，一旦外层把迷你条撑高（QML 预览窗口、或将来某个把它
     * 放进 Fill 区域的容器），多出来的那截就会露出自己的 background(plainBase)
     * —— 表现就是「正在播放条／蓝条下方多出来一条空带」。
     *
     * 所以：
     *   min/max 都写 10.333du（行高），谁也别想把它撑高；
     *   Divider 挪到行内底部 dock（见下），不再作为独立子节点另占高度。
     * 这样整条高度恒 = 内容行高度，结构上就不可能出现多余的一截底色。
     */
    minHeight: ui.sdu(10.333)
    maxHeight: ui.sdu(10.333)
    background: ui.palette.plainBase

    layout: StackLayout {
        orientation: LayoutOrientation.TopToBottom
    }

    Container {
        horizontalAlignment: HorizontalAlignment.Fill
        /*
         * ★ 和列表项同一套参数（官方 ThumbTwoRowListItemVisual）：
         *   行高 10.333du；封面「边长 = 行高」贴左边缘；播放键贴右边缘
         */
        minHeight: ui.sdu(10.333)
        // 行同样不能被撑高：撑高会在「封面 + 标题/艺术家」下方留出一截底色
        maxHeight: ui.sdu(10.333)
        /*
         * 照官方迷你条 NowPlayingMiniControl：整行高 SCREEN_TITLE_HEIGHT = 10du，
         * 封面/播放键都占满行高（上下不加 padding），只有左右 CONTROL_PADDING。
         */



 
        layout: DockLayout {
        }

        /*
         * ★ 底部分隔线 dock 在【行内】底部（原来是根节点的独立子节点，
         *   会额外占一截高度 —— 那截就是多余底色的来源之一）。
         */
        Divider {
            horizontalAlignment: HorizontalAlignment.Fill
            verticalAlignment: VerticalAlignment.Bottom
        }

        // ---- 封面(边长=行高) + 标题/艺术家（点它进正在播放页）----
        Container {
            horizontalAlignment: HorizontalAlignment.Fill
            verticalAlignment: VerticalAlignment.Fill
            // 给右侧播放键让位（播放键边长 = 行高）
            rightPadding: ui.sdu(10.333)
            layout: StackLayout {
                orientation: LayoutOrientation.LeftToRight
            }
            gestureHandlers: [
                TapHandler {
                    onTapped: {
                        npBar.openPlayer()
                    }
                }
            ]

            ImageView {
                // 官方：正方形，边长 = 行高，Fill 贴满整行（和标题/艺术家天然对齐）
                minWidth: ui.sdu(10.333)
                maxWidth: ui.sdu(10.333)
                preferredWidth: ui.sdu(10.333)
                preferredHeight: ui.sdu(10.333)
                verticalAlignment: VerticalAlignment.Fill
                scalingMethod: ScalingMethod.AspectFill
                /*
                 * ★ 必须带上 music.imageCacheVersion：这里是【直接调函数取路径】
                 *   （不像列表那样走模型字段），不引用版本号的话，封面下载完成后
                 *   绑定不会重新求值 —— 表现就是"正在播放页已经显示封面了，
                 *   这条迷你条还一直停在占位图上"。
                 */
                imageSource: (music.imageCacheVersion >= 0
                              && npBar.hasTrack
                              && music.imagePath(music.currentTrack.artUrl).length > 0)
                             ? music.imagePath(music.currentTrack.artUrl)
                             : "asset:///images/ic_default.png"
            }

            Container {
                // 图文间距照列表项（1.5du）
                leftPadding: ui.sdu(1.5)
                verticalAlignment: VerticalAlignment.Center
                layout: StackLayout {
                    orientation: LayoutOrientation.TopToBottom
                }
                Label {
                    /*
                     * ★ 歌词开关（设置页「内容」区）打开【且已经拿到这一句】时
                     *   标题才显示歌词；其余情况一律回落成歌名 ——
                     *   包括：歌词没开 / 歌词还在路上 / 前奏还没到第一句 /
                     *   这首歌压根没歌词（纯音乐）。绝不显示「…」占位。
                     * ★ 只作用于主界面这条迷你条 —— 正在播放全屏页有自己的
                     *   标题栏，两边互不干涉（解耦）。NowPlayingPage 不引用本组件。
                     */
                    text: !npBar.hasTrack ? qsTr("未在播放")
                          : ((music.lyricsEnabled
                              && music.currentLyricLine.length > 0)
                             ? music.currentLyricLine
                             : music.currentTrack.name)
                    textStyle {
                        // 字号和列表项标题一致
                        base: SystemDefaults.TextStyles.SubtitleText
                    }
                }
                Label {
                    /*
                     * 出词后副标题显示翻译（开着翻译且有翻译时）或「歌名 - 作者」；
                     * 其余情况回落成艺术家。
                     * ★ 判定条件必须和标题完全一致（lyricsEnabled 且出词了），
                     *   否则会出现"标题是歌名、副标题是歌名-作者"的错位。
                     */
                    text: !npBar.hasTrack ? qsTr("未在播放")
                          : ((music.lyricsEnabled
                              && music.currentLyricLine.length > 0)
                             ? (music.lyricsTransEnabled
                                && music.currentLyricTrans.length > 0
                                ? music.currentLyricTrans
                                : (music.currentTrack.name + " - "
                                   + music.currentTrack.artistsText))
                             : music.currentTrack.artistsText)
                    textStyle {
                        base: SystemDefaults.TextStyles.SmallText
                        color: ui.palette.textOnPlain
                    }
                }
            }
        }

        // ---- 播放 / 暂停（边长 = 行高，贴右边缘）----
        Container {
            // 播放键同样「边长 = 行高」贴右边缘
            horizontalAlignment: HorizontalAlignment.Right
            verticalAlignment: VerticalAlignment.Fill
            preferredWidth: ui.sdu(10.333)
            preferredHeight: ui.sdu(10.333)
            layout: DockLayout {
            }
            gestureHandlers: [
                TapHandler {
                    onTapped: {
                        if (player.mediaState == MediaState.Started)
                            player.pause()
                        else if (music.playUrlVersion > 0)
                            player.play()
                    }
                }
            ]
            ImageView {
                horizontalAlignment: HorizontalAlignment.Center
                verticalAlignment: VerticalAlignment.Center
                preferredWidth: ui.sdu(6)
                preferredHeight: ui.sdu(6)
                scalingMethod: ScalingMethod.AspectFit
                imageSource: player.mediaState == MediaState.Started
                             ? "asset:///icons/ic_pause.png"
                             : "asset:///icons/ic_play.png"
            }
        }
    }

}
