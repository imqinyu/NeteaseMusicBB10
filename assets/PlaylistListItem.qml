// PlaylistListItem - 歌单 / 专辑列表项（视觉部分）
// 同 SongListItem：缩略图 + 两行文字，占位封面用 ic_default.png

import bb.cascades 1.4

Container {
    id: plItem

    property string title: ""
    property string subtitle: ""
    property string imageSource: ""

    // 选中/高亮：整行铺主题色底（照通讯录联系人列表）
    property bool highlighted: plItem.navigation.wantsHighlight

    /*
     * ★ 严格照官方 Music ui 的 ThumbTwoRowListItemVisual：
     *   - 行高 minHeight + preferredHeight 【双写死】10.333du（LIST_ITEM_VISUAL_HEIGHT）
     *   - 缩略图是「边长 = 行高」的正方形，verticalAlignment: Fill 贴满整行（不内缩）
     *   - 外层【不设】左 padding —— 图紧贴左边缘；留白交给右边文字容器
     */
    minHeight: ui.sdu(10.333)
    // ★ 不写死 preferredHeight（我们的字号塞不下 10.333du，会压行错位）
    background: plItem.highlighted ? ui.palette.primary : Color.Transparent
    // ★ 关掉 Cascades 默认的焦点红框：高亮只由上面那层整行底色负责（照通讯录）
    navigation {
        defaultHighlightEnabled: false
    }

    layout: StackLayout {
        orientation: LayoutOrientation.LeftToRight
    }

    ImageView {
        // 官方：正方形，边长 = 行高，Fill 贴满整行高（所以图和文字天然对齐）
        minWidth: ui.sdu(10.333)
        maxWidth: ui.sdu(10.333)
        preferredWidth: ui.sdu(10.333)
        preferredHeight: ui.sdu(10.333)
        verticalAlignment: VerticalAlignment.Fill
        scalingMethod: ScalingMethod.AspectFill
        imageSource: plItem.imageSource.length > 0
                     ? plItem.imageSource
                     : "asset:///images/ic_default.png"
    }

    Container {
        // 官方：图文间距 = 容器 leftPadding 1du + 标题 leftMargin 0.5du = 1.5du；右侧 2du
        leftPadding: ui.sdu(1.5)
        rightPadding: ui.sdu(2)
        verticalAlignment: VerticalAlignment.Center
        layoutProperties: StackLayoutProperties {
            spaceQuota: 1
        }
        layout: StackLayout {
            orientation: LayoutOrientation.TopToBottom
        }
        Label {
            text: plItem.title
            /*
             * ★ 间距/字号照官方 TrackInfoLabel（和 SongListItem 完全一致）：
             *   标题【底部对齐】把字体行高多余的那段空白推到上方，副标题
             *   再 topMargin 0 贴上来 —— 否则两行之间会空出一截。
             */
            verticalAlignment: VerticalAlignment.Bottom
            bottomMargin: 0
            textStyle {
                fontSize: FontSize.Medium
                fontWeight: FontWeight.Normal
                color: plItem.highlighted
                       ? ui.palette.textOnPrimaryDark
                       : SystemDefaults.Paints.defaultText
            }
        }
        Label {
            text: plItem.subtitle
            // ★ 同上：topMargin 0 紧贴标题
            topMargin: 0
            textStyle {
                fontSize: FontSize.Small
                color: plItem.highlighted
                       ? ui.palette.secondaryTextOnPrimaryDark
                       : ui.palette.textOnPlain
            }
        }
    }
}
