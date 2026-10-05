// LoginPage - 登录页（由 NavigationPane push 进来）
//
// 登录成功后自动 pop 回上一页 —— 不需要「取消」按钮，
// 返回交给 BB10 原生的返回键 / 左滑（这也是 ModPlayer 的做法）。

import bb.cascades 1.3

Page {
    id: loginPage
    objectName: "loginPage"

    /*!
     * 本页自己的输入错误（剪贴板空 / 没填就点完成）。
     * ★ C++ 的 lastLoginError 只报"校验结果"，输入侧的提示留在 QML ——
     *   C++ 里写中文要走 NmTr 的十六进制，没必要为几句话折腾。
     */
    property string inputError: ""
actions: [ActionItem {
            title: qsTr("完成")
            imageSource: "asset:///icons/ic_add_contact.png"
            ActionBar.placement: ActionBarPlacement.Signature
            // 校验期间灰掉，免得连点反复提交（music.loading 由 login() 打开）
            enabled: !music.loading
            onTriggered: {
                var text = cookieTextArea.text ? cookieTextArea.text.trim() : ""
                if (text.length === 0) {
                    loginPage.inputError = qsTr("请先点「粘贴」，或直接把 MUSIC_U 的值填进来")
                    return
                }
                loginPage.inputError = ""
                // 立即返回；结果通过 loggedIn / lastLoginError 反映。
                // 成功后由 main.qml 的 onLoggedInNowChanged 自动 pop 本页。
                music.login(text)
            }

        },
ActionItem {
            title: qsTr("粘贴")
            imageSource: "asset:///icons/ic_review_add.png"
            ActionBar.placement: ActionBarPlacement.OnBar
            onTriggered: {
                // 读系统剪贴板（text/plain）。返回空串 = 剪贴板里没有文本
                var text = music.clipboardText()
                if (!text || text.length === 0) {
                    loginPage.inputError = qsTr("剪贴板里没有文本：请先在电脑浏览器的开发者工具里复制 MUSIC_U 或整段 Cookie")
                    return
                }
                cookieTextArea.text = text
                loginPage.inputError = ""
            }

        }]
    // 登录成功后的自动返回放在主页面做（那里才拿得到 NavigationPane；
    // 子页面里存 nav 对象会踩 "Unable to set property"）

//    titleBar: TitleBar {
//        title: qsTr("登录")
//        acceptAction: ActionItem {
//            title: qsTr("登录")
//            onTriggered: {
//                music.login(cookieTextArea.text)
//            }
//        }
//    }

    Container {
        topPadding: 20
        leftPadding: 20
        rightPadding: 20
        layout: StackLayout {
            orientation: LayoutOrientation.TopToBottom
        }

        Label {
            text: qsTr("在电脑浏览器登录 music.163.com 后"
                       + "\n点击开发者工具 → Application → Cookies "
                       + "\n复制 MUSIC_U 的值（或整段 Cookie）")
            multiline: true
            textStyle {
                base: SystemDefaults.TextStyles.BodyText
            }
        }

        TextArea {
            id: cookieTextArea
            preferredHeight: 200
            inputMode: TextAreaInputMode.Text
            // 用户一改输入就把上一次的红字清掉（errorChanged 也行，但
            // TextArea 只有 textChanging 这个"用户输入"信号）
            onTextChanging: loginPage.inputError = ""
        }

        Container {
            visible: music.loading
            topPadding: 8
            layout: StackLayout {
                orientation: LayoutOrientation.LeftToRight
            }
            ActivityIndicator {
                running: true
                preferredWidth: 36
                preferredHeight: 36
                verticalAlignment: VerticalAlignment.Center
            }
            Label {
                text: qsTr("正在校验登录态…")
                verticalAlignment: VerticalAlignment.Center
                textStyle {
                    base: SystemDefaults.TextStyles.SmallText
                }
            }
        }

        Label {
            // 本页的输入错误优先，否则显示 C++ 回来的校验结果
            //（"没有从输入中找到 MUSIC_U" / "登录已失效，请重新粘贴"）
            text: loginPage.inputError.length > 0
                  ? loginPage.inputError : music.lastLoginError
            visible: text.length > 0
            multiline: true
            textStyle {
                base: SystemDefaults.TextStyles.SmallText
                color: Color.create("#ff6b6b")
            }
        }
    }
}
