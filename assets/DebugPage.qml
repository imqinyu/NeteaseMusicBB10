// DebugPage - 接口调试日志页（由应用菜单 push 进来）

import bb.cascades 1.3

Page {
    id: debugPage

    objectName: "debugPage"
    // 媒体服务状态由主页面在 push 后填进来（NowPlayingConnection 在主页面里，
    // 独立组件访问不到它的 id）
    property string mediaStatusText: ""

    titleBar: TitleBar {
        title: qsTr("接口调试日志")
    }

    function refresh() {
        debugTextArea.text = music.debugLog()
    }

    onCreationCompleted: {
        music.setDebugEnabled(true)
        refresh()
    }

    Container {
        leftPadding: 10
        rightPadding: 10
        topPadding: 10
        // 媒体服务诊断：按音量键没出现播放控件时先看这行
        Label {
            text: qsTr("媒体服务：") + debugPage.mediaStatusText
            multiline: true
            textStyle {
                base: SystemDefaults.TextStyles.SmallText
            }
        }
        TextArea {
            id: debugTextArea
            preferredHeight: 600
            editable: false
            textStyle {
                base: SystemDefaults.TextStyles.SmallText
            }
        }
    }

    actions: [
        ActionItem {
            title: qsTr("刷新")
            ActionBar.placement: ActionBarPlacement.OnBar
            onTriggered: {
                debugPage.refresh()
            }
        }
    ]
}
