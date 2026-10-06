// BarcodeLoginPage - 扫码登录
//
// 打开本页即向服务端要一个二维码 key，C++ 把登录地址本地画成二维码
// （见 NmQrCode 头注释：为什么自己画而不是让服务端出图），之后每 3 秒
// 问一次状态：
//
//     801 等待扫码  →  802 已扫码、待手机确认  →  803 授权成功（自动完成登录）
//     800 二维码已过期 —— 点「刷新」重新取一个
//
// 登录成功后由 main.qml 的 onLoggedInNowChanged 把本页和下面的登录页
// 一起弹掉（本页是叠在登录页之上推的）。
//
// 轮询不需要手动停：二维码过期时服务端会回 800，C++ 收到就停；
// 授权成功 / 失败同样会停。所以就算直接返回也不会一直在后台发请求。

import bb.cascades 1.3

Page {
    id: barcodePage
    objectName: "barcodeLoginPage"

    actions: [
        ActionItem {
            // 刷新二维码：过期了、或想换一个时用
            title: qsTr("刷新")
            imageSource: "asset:///icons/ic_reload.png"
            ActionBar.placement: ActionBarPlacement.OnBar
            onTriggered: {
                music.refreshQrLogin()
            }
        }
    ]

    Container {
        horizontalAlignment: HorizontalAlignment.Fill
        verticalAlignment: VerticalAlignment.Fill
        leftPadding: 40
        rightPadding: 40
        layout: DockLayout {
        }

        Container {
            horizontalAlignment: HorizontalAlignment.Center
            verticalAlignment: VerticalAlignment.Center

            ImageView {
                horizontalAlignment: HorizontalAlignment.Center
                preferredWidth: 460
                preferredHeight: 460
                scalingMethod: ScalingMethod.AspectFit
                // 还没拿到二维码时是空串，ImageView 什么都不画
                imageSource: music.qrImagePath
            }

            Label {
                horizontalAlignment: HorizontalAlignment.Center
                topMargin: 30
                multiline: true
                text: music.qrStatusText
                textStyle {
                    base: SystemDefaults.TextStyles.BodyText
                    textAlign: TextAlign.Center
                }
            }
        }
    }

    /*
     * 一进页面就开始取二维码。
     *
     * ★ 用 onCreationCompleted —— Cascades 【没有】 Component 这个附加对象，
     *   写 Component.onCompleted 会让整个 QML 文件加载失败
     *   （真机日志 "Non-existent attached object"，页面根本推不出来）。
     */
    onCreationCompleted: {
        music.startQrLogin()
    }
}
