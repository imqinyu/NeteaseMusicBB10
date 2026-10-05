// PropertiesPage - 音乐详情（照官方 Music ui 的 Properties 页，只保留文本信息）
//
// ★ 版式很简单：一行一个「项目名 / 值」。
//   歌曲名 / 作者 / ID / 长度 / 所属专辑 来自 currentTrack；
//   作词、作曲、编曲等是从【歌词原文】里读出来的
//   （见 MusicController::extractLyricCredits），有才显示，没有就整行不出。
//
// ★ 页面里用 SegmentedControl 分「详情 / 歌词」两块：
//   歌词那块【原样显示 LRC 文本】，不做任何渲染 —— 就是"只显示文本 lrc 文件内容"。
//   「查看歌词」动作 = 切到那块。

import bb.cascades 1.4

Page {
    id: propPage
    objectName: "propertiesPage"

//titlebar删了 没必要 黑莓屏幕空间小 空间很宝贵

    property string mode: "info"

    /*
     * 详情列表是否已经有内容。
     * ★ ArrayDataModel.size() 是【方法】不是属性，绑定不会随内容填充重算 ——
     *   页面刚建好时它求值为 0 就永远停在 0，于是明明有内容也会一直显示
     *   "当前没有播放中的歌曲"（真机反馈）。所以自己拿一个 bool 记。
     */
    property bool hasProps: false

    /*! 歌词正文（详情目标的歌词是异步拉的，靠下面版本号重取） */
    property string lyricBody: ""

    /*
     * ★ 详情目标变化、或它的歌词异步到达时，C++ 会把版本号 +1。
     *   歌词是异步来的，进页面那一刻通常还没到 —— 不重取的话详情里的
     *   "作词 / 作曲"和歌词分段会一直是空的。
     */
    property int propsVersion: music.propertiesVersion
    onPropsVersionChanged: {
        propPage.reload()
    }

    /*
     * ★ 本页【不挂 actions】—— 顶部已经有「详情 / 歌词」分段按钮，
     *   再挂 action 是重复的。
     *   （原来的「保存歌词」已按需求移除：系统保存卡片在这台设备上始终
     *     匹配不到处理器，留着也只是占位置。）
     */

    Container {
        layout: StackLayout {
            orientation: LayoutOrientation.TopToBottom
        }

        SegmentedControl {
            id: modeControl
            onSelectedValueChanged: {
                propPage.mode = selectedValue
            }
            Option {
                text: qsTr("详情")
                value: "info"
                selected: true
            }
            Option {
                text: qsTr("歌词")
                value: "lyrics"
            }
        }

        // 空状态（没在播歌时详情是空的）
        Label {
            // ★ 用 hasProps，见上面那条说明（size() 绑不上）
            visible: propPage.mode == "info" && ! propPage.hasProps
            // ★ Label 不支持 padding（会刷 "Padding is not supported" 警告），用 margin
            leftMargin: ui.sdu(2)
            rightMargin: ui.sdu(2)
            topMargin: ui.sdu(3)
            text: qsTr("当前没有播放中的歌曲")
            multiline: true
            textStyle {
                base: SystemDefaults.TextStyles.BodyText
                color: ui.palette.textOnPlain
            }
        }

        // ---- 详情：一行一个「项目名 / 值」----
        ListView {
            visible: propPage.mode == "info"
            layoutProperties: StackLayoutProperties {
                spaceQuota: 1
            }
            dataModel: propModel
            listItemComponents: [
                ListItemComponent {
                    type: ""
                    /*
                     * 版式照官方 mm.extension 的 PropertyItem：
                     *   · 项目名：FontSize.Small + 灰色（次要信息）
                     *   · 值    ：FontSize.Large —— 【值比项目名大】是官方属性页的特征
                     *   · 每行底部一条 Divider 分隔线
                     *   · 左右 padding 2du、下 1du，内容垂直居中
                     * ★ 官方用的是 fontSize/fontWeight 直给，不是 TextStyles 那套
                     *   （换成 TextStyles 字号会明显偏小，就是之前"和官方不像"的原因）。
                     */
                    Container {
                        layout: DockLayout {}

                        Container {
                            leftPadding: ui.sdu(2)
                            rightPadding: ui.sdu(2)
                            bottomPadding: ui.sdu(1)
                            horizontalAlignment: HorizontalAlignment.Fill
                            verticalAlignment: VerticalAlignment.Center
                            layout: StackLayout {}

                            Label {
                                text: ListItemData.title
                                horizontalAlignment: HorizontalAlignment.Fill
                                textStyle {
                                    fontSize: FontSize.Small
                                    fontWeight: FontWeight.W400
                                    color: Color.Gray
                                }
                                textFormat: TextFormat.Plain
                                topMargin: 0
                                bottomMargin: 0
                            }
                            Label {
                                text: ListItemData.description
                                multiline: true
                                horizontalAlignment: HorizontalAlignment.Fill
                                textStyle {
                                    fontSize: FontSize.Large
                                    fontWeight: FontWeight.W400
                                }
                                textFormat: TextFormat.Plain
                                topMargin: 0
                                bottomMargin: 0
                            }
                        }

                        Divider {
                            verticalAlignment: VerticalAlignment.Bottom
                        }
                    }
                }
            ]
        }

        // ---- 歌词：原样显示 LRC 文本 ----
        ScrollView {
            visible: propPage.mode == "lyrics"
            layoutProperties: StackLayoutProperties {
                spaceQuota: 1
            }
            scrollViewProperties {
                scrollMode: ScrollMode.Vertical
            }
            Container {
                leftPadding: ui.sdu(2)
                rightPadding: ui.sdu(2)
                topPadding: ui.sdu(2)
                bottomPadding: ui.sdu(2)
                Label {
                    text: propPage.lyricBody
                    multiline: true
                    textStyle {
                        base: SystemDefaults.TextStyles.SmallText
                    }
                }
            }
        }
    }

    attachedObjects: [
        ArrayDataModel {
            id: propModel
        }
    ]

    onCreationCompleted: {
        propPage.reload()
    }

    /*! 重取一次快照（进页面时、以及详情目标的歌词异步到达时各调一次） */
    function reload() {
        propModel.clear()
        var list = music.songProperties()
        for (var i = 0; i < list.length; i ++)
            propModel.append(list[i])
        propPage.hasProps = (propModel.size() > 0)
        propPage.lyricBody = music.propertiesLyric()
    }
}
