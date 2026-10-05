// AppCover - 720x720 机型专用（Q20 / Q10 / Classic 等方形设备）
//
// 版式和默认版一致，数值按 720x720 定：内边距 20 / 上 10 / 下 16。
//
// ★ 结构是【两个元素】：占位层（纯黑，未在播放/没封面，从启动就 visible）
//   + 封面层（真封面）。合成就没有第一帧了 —— 静态状态下封面补不回来。
// ★ 信息条用 minHeight 给下限，不要 preferredHeight（后者会把高度写死、很难看）。
// ★ imageSource 永远非空（C++ 的 coverImagePath 已保证）。
// ★ scalingMethod 只能用字面量（写三元会报 "Invalid scaling method."）。
// ★ 别加 maxLineCount（Label 没这个属性，会导致整个文件加载失败）。
// ★★ 绝对不要用 ui.sdu() —— cover 的 content 里取不到 `ui`，一用就整片黑。
// ★ content 里不要引用【外层】id；不要引用 `player`；也不要用 Connections。

import bb.cascades 1.4

SceneCover {
    id: coverRoot

    content: Container {
        background: Color.Black
        horizontalAlignment: HorizontalAlignment.Fill
        verticalAlignment: VerticalAlignment.Fill
        layout: DockLayout {
        }

        // ---- 占位层：没封面 / 未在播放 ----
        Container {
            visible: ! music.coverHasArt
            horizontalAlignment: HorizontalAlignment.Fill
            verticalAlignment: VerticalAlignment.Fill
            background: Color.Black
        }

        // ---- 封面层：铺满 ----
        ImageView {
            visible: music.coverHasArt
            horizontalAlignment: HorizontalAlignment.Fill
            verticalAlignment: VerticalAlignment.Fill
            scalingMethod: ScalingMethod.AspectFill
            imageSource: music.coverImagePath
        }

        // ---- 底部信息条（minHeight 是下限）----
        Container {
            horizontalAlignment: HorizontalAlignment.Fill
            verticalAlignment: VerticalAlignment.Bottom
            background: Color.create("#CC000000")
            minHeight: 130
            leftPadding: 20
            rightPadding: 20
            topPadding: 10
            bottomPadding: 16
            layout: StackLayout {
                orientation: LayoutOrientation.TopToBottom
            }
            Label {
                text: music.coverTitle
                multiline: true
                textStyle {
                    base: SystemDefaults.TextStyles.TitleText
                    color: Color.White
                }
            }
            Label {
                text: music.coverSubtitle
                multiline: true
                textStyle {
                    base: SystemDefaults.TextStyles.SubtitleText
                    color: Color.create("#cccccc")
                }
            }
        }
    }
}
