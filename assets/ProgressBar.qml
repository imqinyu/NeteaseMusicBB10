// ProgressBar - 播放时间条
//
// ★ 结构完全照官方 Music ui 的 TimeSlider.qml（imports/mm/extension/internal，纯 QML）：
//     Container(TopToBottom) {
//         Slider { horizontalAlignment:Fill; verticalAlignment:Center; ... }
//         Container(DockLayout, 左右各 2du 内边距) {
//             Label { 左，已播时间 }
//             Label { 右，"-剩余时间" }
//         }
//     }
//   官方那份多了 timeSliderControllerRef（C++ 控制器）来驱动，这里换成直接绑定
//   player.duration / player.position；拖动松手 → music.seekTo()。

import bb.cascades 1.4

Container {
    id: sliderBox

    property int duration: 1
    property int position: 0

    function fmt(ms) {
        if (ms < 0)
            ms = 0
        var t = Math.floor(ms / 1000)
        var m = Math.floor(t / 60)
        var s = t % 60
        return m + ":" + (s < 10 ? "0" + s : "" + s)
    }

    layout: StackLayout {
        orientation: LayoutOrientation.TopToBottom
    }

    Slider {
        id: sliderBar
        horizontalAlignment: HorizontalAlignment.Fill
        verticalAlignment: VerticalAlignment.Center
        fromValue: 0
        toValue: sliderBox.duration > 0 ? sliderBox.duration : 1

        // 用户是否正在拖动（拖动期间别被 player.position 拉回去）
        property bool userSeeking: false

        onTouch: {
            if (event.isDown()) {
                sliderBar.userSeeking = true
                // ★ 让 C++ 在拖动期间暂停位置更新（照官方 setSamplingMode）：
                //   否则位置一动，onPositionChanged 就把滑块拽回去 →「拖不动」
                music.setSeeking(true)
            } else if (event.isUp() || event.isCancel()) {
                sliderBar.userSeeking = false
                music.setSeeking(false)
                music.seekTo(Math.round(sliderBar.immediateValue))
                sliderBox.updateLabels()
            }
        }
        onImmediateValueChanged: {
            sliderBox.updateLabels()
        }
    }

    Container {
        layout: DockLayout {
        }
        horizontalAlignment: HorizontalAlignment.Fill
        verticalAlignment: VerticalAlignment.Center
        leftPadding: ui.sdu(2)
        rightPadding: ui.sdu(2)

        Label {
            id: time
            horizontalAlignment: HorizontalAlignment.Left
            text: ""
            textStyle {
                base: SystemDefaults.TextStyles.SubtitleText
                fontSize: FontSize.PointValue
                fontSizeValue: 6
                color: ui.palette.primary
            }
            textFormat: TextFormat.Plain
        }
        Label {
            id: durationLabel
            horizontalAlignment: HorizontalAlignment.Right
            text: ""
            textStyle {
                base: SystemDefaults.TextStyles.SubtitleText
                fontSize: FontSize.PointValue
                fontSizeValue: 6
                color: ui.palette.textOnPlain
            }
            textFormat: TextFormat.Plain
        }
    }

    function updateLabels() {
        var p = sliderBar.userSeeking ? sliderBar.immediateValue : sliderBox.position
        time.text = fmt(p)
        durationLabel.text = "-" + fmt(sliderBox.duration - p)
    }

    onPositionChanged: {
        if (! sliderBar.userSeeking) {
            sliderBar.value = sliderBox.position
            updateLabels()
        }
    }
    onDurationChanged: {
        updateLabels()
    }
    onCreationCompleted: {
        sliderBar.value = sliderBox.position
        updateLabels()
    }
}
