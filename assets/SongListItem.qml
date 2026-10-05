// SongListItem - 歌曲列表项（视觉部分）
//
// 照系统音乐播放器的 ThumbTwoRowListItemVisual 写的：
//   正方形缩略图（行高）+ 右侧两行文字（标题 / 副标题），右侧再挂时长。
// 占位封面用系统播放器那张 ic_default.png。
//
// ⚠️ contextActions（长按菜单）不放在这里 —— ListItem.view 只能在
//   列表项视觉的根节点上取，放在组件内部会报 "not the root node"。
//   菜单写在各页面的 ListItemComponent 里。

import bb.cascades 1.4

Container {
    id: songItem

    property string title: ""
    property string subtitle: ""
    property string imageSource: ""
    property string durationText: ""
    property bool isVip: false
    property bool hasMv: false
    /*! 是否正在播放（列表里高亮当前曲目） */
    property bool playing: false

    /*
     * 选中/高亮：照通讯录联系人列表 —— 整行铺主题色底、文字转白，
     * 而不是只有一圈边框。wantsHighlight 由高亮中的列表项向下传播。
     */
    property bool highlighted: songItem.navigation.wantsHighlight

    /*! 是否显示左侧封面（官方「全部歌曲」不显示封面，省下载时间） */
    property bool showImage: true

    /*
     * ★ 严格照官方 Music ui：
     *   SongListItemVisual（无封面，如全部歌曲）：行高 10.333du、外层左右 padding 2du
     *   ThumbTwoRowListItemVisual（有封面）：封面「边长 = 行高」贴左边缘、外层左 padding 0
     */
    minHeight: ui.sdu(10.333)
    // ★ 不写死 preferredHeight：官方那套字号比我们的小，硬写 10.333du
    //   两行字塞不下就会互相压过去（表现为行与行错位）。
    background: songItem.highlighted ? ui.palette.primary : Color.Transparent
    // ★ 关掉 Cascades 默认的焦点红框：高亮只由上面那层整行底色负责（照通讯录）
    navigation {
        defaultHighlightEnabled: false
    }
    leftPadding: songItem.showImage ? 0 : ui.sdu(2)
    rightPadding: ui.sdu(2)

    layout: StackLayout {
        orientation: LayoutOrientation.LeftToRight
    }

    ImageView {
        visible: songItem.showImage
        // 官方：正方形，边长 = 行高，Fill 贴满整行高（图与文字天然对齐）
        minWidth: ui.sdu(10.333)
        maxWidth: ui.sdu(10.333)
        preferredWidth: ui.sdu(10.333)
        preferredHeight: ui.sdu(10.333)
        // 空/没下完时用占位图，避免 "Unrecognized scheme" 告警
        imageSource: songItem.imageSource.length > 0
                     ? songItem.imageSource
                     : "asset:///images/ic_default.png"
        scalingMethod: ScalingMethod.AspectFill
        verticalAlignment: VerticalAlignment.Fill
    }

    Container {
        // ★ 照官方 ThumbTwoRowListItemVisual：封面和文字之间只隔 0.5du
        leftPadding: ui.sdu(0.5)
        verticalAlignment: VerticalAlignment.Center
        layoutProperties: StackLayoutProperties {
            spaceQuota: 1
        }
        layout: StackLayout {
            orientation: LayoutOrientation.TopToBottom
        }
        Label {
            text: songItem.title
            /*
             * ★★ 官方 TrackInfoLabel 的关键一笔：标题【底部对齐】。
             *   Label 的高度是整个字体行高（字形上下都有留白），默认对齐时
             *   标题下方会空出一截 —— 看起来就是"歌名和歌手隔太开"。
             *   压到底部后，那截空白就跑到标题上方去了，两行自然靠拢。
             */
            verticalAlignment: VerticalAlignment.Bottom
            bottomMargin: 0
            textStyle {
                /*
                 * 官方 MusicListView 的 titleNormal 是 FontUtils.PrimaryR。
                 * ★ 用 fontSize 直给，不走 TextStyles —— TextStyles 自带的
                 *   默认行距/字重会把这一行整体撑高，和官方观感对不上
                 *   （Properties 详情页那边就是这么修正过来的）。
                 */
                fontSize: FontSize.Medium
                // 正在播放的曲目：加粗 + 转【主题色】（只加粗不够显眼）
                fontWeight: songItem.playing ? FontWeight.Bold : FontWeight.Normal
                color: songItem.highlighted
                       ? ui.palette.textOnPrimaryDark
                       : (songItem.playing ? ui.palette.primary
                                           : SystemDefaults.Paints.defaultText)
            }
        }
        Label {
            text: songItem.subtitle
            // ★ 同上：官方副标题是 topMargin 0，紧贴标题底部
            topMargin: 0
            textStyle {
                // 官方 subtitleNormal 是 FontUtils.P3L（比标题小一档）
                fontSize: FontSize.Small
                fontWeight: FontWeight.Normal
                // 副标题（歌手 · 专辑）同样跟着转主题色
                color: songItem.highlighted
                       ? ui.palette.secondaryTextOnPrimaryDark
                       : (songItem.playing ? ui.palette.primary
                                           : ui.palette.textOnPlain)
            }
        }
    }

    Container {
        verticalAlignment: VerticalAlignment.Center
        layout: StackLayout {
            orientation: LayoutOrientation.LeftToRight
        }
        Label {
            visible: songItem.hasMv
            text: qsTr("MV")
            rightMargin: ui.sdu(1)
            textStyle {
                // 官方 durationNormal 是 FontUtils.P2R（比副标题再小一档）
                fontSize: FontSize.XSmall
                fontWeight: FontWeight.Normal
                color: ui.palette.primary
            }
        }
        Label {
            visible: songItem.isVip
            text: qsTr("VIP")
            rightMargin: ui.sdu(1)
            textStyle {
                // 官方 durationNormal 是 FontUtils.P2R（比副标题再小一档）
                fontSize: FontSize.XSmall
                fontWeight: FontWeight.Normal
                color: Color.create("#ff6b6b")
            }
        }
        Label {
            text: songItem.durationText
            textStyle {
                // 官方 durationNormal 是 FontUtils.P2R（比副标题再小一档）
                fontSize: FontSize.XSmall
                fontWeight: FontWeight.Normal
                color: songItem.highlighted
                       ? ui.palette.secondaryTextOnPrimaryDark
                       : ui.palette.textOnPlain
            }
        }
    }
}
