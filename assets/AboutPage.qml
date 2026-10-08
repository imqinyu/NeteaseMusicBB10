// AboutPage - 关于（应用菜单的 helpAction 打开）

import bb.cascades 1.4

Page {
    id: aboutPage
    objectName: "aboutPage"

    titleBar: TitleBar {
        title: qsTr("关于")
    }

    ScrollView {
        Container {
            topPadding: ui.sdu(3)
          
            layout: StackLayout {
                orientation: LayoutOrientation.TopToBottom
            }

            ImageView {
                horizontalAlignment: HorizontalAlignment.Center
                preferredWidth: ui.sdu(20)
                preferredHeight: ui.sdu(20)
                imageSource: "asset:///icons/icon.png"
                scalingMethod: ScalingMethod.AspectFill
            }

            Label {
                topMargin: ui.sdu(2)
                horizontalAlignment: HorizontalAlignment.Center
                text: qsTr("轮易云音乐BB10")
                textStyle {
                    base: SystemDefaults.TextStyles.TitleText
                }
            }

            Header {
                title: qsTr("说明")
            }
            Label {
                multiline: true

                text: "网易云音乐第三方版 仅供学习使用\r\n免费分享 禁止倒卖\r\n欢迎加入BlackBerryPhonix\r\nQQ交流群:892633534"
                textStyle {
                    base: SystemDefaults.TextStyles.BodyText
                }
            }

           
            Header {
                title: qsTr("开发团队")
            }
            Label {
                multiline: true

                text: "UI原型、QA：Fremantle\r\n编程：Hy4、Hy3、Deepseek V4.1、Kimi K3\r\n图标设计：ChatGPT"
                textStyle {
                    base: SystemDefaults.TextStyles.BodyText
                }
            }
            Header {
                title: qsTr("鸣谢")
            }
            Label {
                multiline: true

                text: "排名不分先后\r\nOleksandr：提供越狱漏洞和工具\r\nPablo from WaitBerry:提供参考文档\r\nGithub NeteaseCloudMusicApiEnhanced项目：提供网易云接口\r\nAnwar Ludin、Davenson Lombard：提供免费的黑莓开发教程\r\n以及所有支持我的AI朋友们"
                textStyle {
                    base: SystemDefaults.TextStyles.BodyText
                }
            }
        }
    }

//    actions: [
//        ActionItem {
//            title: qsTr("接口调试日志")
//            imageSource: "asset:///icons/ic_help.png"
//            ActionBar.placement: ActionBarPlacement.OnBar
//            onTriggered: {
//                music.setDebugEnabled(true)
//            }
//        }
//    ]
}
