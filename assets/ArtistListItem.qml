// ArtistListItem - 艺人列表项（视觉部分）
// 头像用圆形裁切效果：Cascades 没有圆角属性，用 AspectFill + 固定尺寸即可；
// 占位头像同样用 ic_default.png

import bb.cascades 1.4

Container {
    id: artistItem

    property string title: ""
    property string subtitle: ""
    property string imageSource: ""

    // 选中/高亮：整行铺主题色底（照通讯录联系人列表）
    property bool highlighted: artistItem.navigation.wantsHighlight

    /*
     * ★ 严格照官方 Music ui 的 ThumbTwoRowListItemVisual：
     *   - 行高 minHeight + preferredHeight 【双写死】10.333du
     *   - 头像「边长 = 行高」，Fill 贴满整行；外层不设左 padding
     */
    minHeight: ui.sdu(10.333)
    // ★ 不写死 preferredHeight（我们的字号塞不下 10.333du，会压行错位）
    background: artistItem.highlighted ? ui.palette.primary : Color.Transparent
    // ★ 关掉 Cascades 默认的焦点红框：高亮只由上面那层整行底色负责（照通讯录）
    navigation {
        defaultHighlightEnabled: false
    }

    layout: StackLayout {
        orientation: LayoutOrientation.LeftToRight
    }

    ImageView {
        // 官方：正方形，边长 = 行高，Fill 贴满整行高
        minWidth: ui.sdu(10.333)
        maxWidth: ui.sdu(10.333)
        preferredWidth: ui.sdu(10.333)
        preferredHeight: ui.sdu(10.333)
        verticalAlignment: VerticalAlignment.Fill
        scalingMethod: ScalingMethod.AspectFill
        imageSource: artistItem.imageSource.length > 0
                     ? artistItem.imageSource
                     : "asset:///images/avatar_bright.png"
    }

    Container {
        // 官方：图文间距 1.5du；右侧 2du
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
            text: artistItem.title
            /*
             * ★ 间距/字号照官方 TrackInfoLabel（和 SongListItem /
             *   PlaylistListItem 完全一致）：标题【底部对齐】把字体行高多余的
             *   那截空白推到上方，副标题再 topMargin 0 贴上来 ——
             *   否则两行之间会空出一段，看着"名字和副标题隔太开"。
             */
            verticalAlignment: VerticalAlignment.Bottom
            bottomMargin: 0
            textStyle {
                fontSize: FontSize.Medium
                fontWeight: FontWeight.Normal
                color: artistItem.highlighted
                       ? ui.palette.textOnPrimaryDark
                       : SystemDefaults.Paints.defaultText
            }
        }
        Label {
            text: artistItem.subtitle
            // ★ 同上：topMargin 0 紧贴标题
            topMargin: 0
            textStyle {
                fontSize: FontSize.Small
                color: artistItem.highlighted
                       ? ui.palette.secondaryTextOnPrimaryDark
                       : ui.palette.textOnPlain
            }
        }
    }
}
