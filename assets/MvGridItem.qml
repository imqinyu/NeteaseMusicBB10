// MvGridItem - MV 网格项（视觉部分）
//
// 网格排布照系统视频/音乐应用：FlowListLayout + FlowListLayoutProperties
// （aspectRatio + fillRatio = 1/列数），单元格是「封面 + 上下两行文字」。

import bb.cascades 1.4

Container {
    id: mvItem

    property string title: ""
    property string subtitle: ""
    property string imageSource: ""

    // 网格布局属性由使用方挂在外层列表项的根节点上（这里挂了不生效）
    topPadding: ui.sdu(1)
    bottomPadding: ui.sdu(1)
    leftPadding: ui.sdu(1)
    rightPadding: ui.sdu(1)

    layout: StackLayout {
        orientation: LayoutOrientation.TopToBottom
    }

    ImageView {
        horizontalAlignment: HorizontalAlignment.Fill
        preferredHeight: ui.sdu(16)
        imageSource: mvItem.imageSource.length > 0
                     ? mvItem.imageSource
                     : "asset:///images/ic_default.png"
        scalingMethod: ScalingMethod.AspectFill
    }

    Label {
        topMargin: ui.sdu(0.5)
        text: mvItem.title
        textStyle {
            base: SystemDefaults.TextStyles.SubtitleText
        }
    }

    Label {
        text: mvItem.subtitle
        textStyle {
            base: SystemDefaults.TextStyles.SmallText
            color: ui.palette.textOnPlain
        }
    }
}
