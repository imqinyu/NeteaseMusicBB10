// AccountManagePage - 账号管理（多账号）
//
// 骨架照 NMDemoUI/assets/AccountManagePage.qml 的草图：
//   「新增账号」进登录页；分「当前账号」「已登录」两段；
//   每项长按 → 复制凭据 / 退出（当前账号）或 登录（其它账号）。
//
// 数据：
//   music.currentAccount   当前账号（QVariantMap，未登录是空 map）
//   music.otherAccounts    其它已保存账号（ArrayDataModel）
// 凭证存在 NmSession（QSettings）里，列表里拿到的只有打码后的 masked；
// 要明文（复制凭据）走 music.accountCredential(id)，取完立刻丢给剪贴板。
//
// ⚠️ 和别的独立页一样：委托里看不到 context property `music`，
//    所以 ListView 上挂了 `property variant controller: music`。

import bb.cascades 1.4

Page {
    id: accountPage
    objectName: "accountManagePage"

    titleBar: TitleBar {
        title: qsTr("账号管理")
    }

    // 供 main.qml 的 pop 处理把 ListView 从模型上摘下来（缓解返回卡顿）
    property alias myList: otherList

    actions: [
        ActionItem {
            title: qsTr("新增账号")
            imageSource: "asset:///icons/ic_add.png"
            ActionBar.placement: ActionBarPlacement.Signature
            onTriggered: {
                // 独立页拿不到 main 的 root，走「请求 + 版本号」让 main 推登录页
                music.requestOpenLogin()
            }
        }
    ]

    // 当前账号那一行的 id（空串 = 还没校验出 userId 的账号，操作时由 C++ 兜底）
    property string currentId: music.currentAccount && music.currentAccount.id
                               ? music.currentAccount.id : ""
    property bool hasCurrent: music.currentAccount
                              && music.currentAccount.id !== undefined
                              && music.currentAccount.nickName !== undefined

    Container {
        layout: StackLayout {
            orientation: LayoutOrientation.TopToBottom
        }

        // ===================== 当前账号 =====================
        Header {
            title: qsTr("当前账号")
        }

        Container {
            leftPadding: ui.sdu(2)
            rightPadding: ui.sdu(2)
            topPadding: ui.sdu(1)
            bottomPadding: ui.sdu(1)
            layout: StackLayout {
                orientation: LayoutOrientation.LeftToRight
            }
            contextActions: [
                ActionSet {
                    title: accountPage.hasCurrent
                           ? music.currentAccount.nickName : qsTr("当前账号")
                    subtitle: accountPage.hasCurrent
                              ? music.currentAccount.masked : qsTr("未登录")
                    actions: [
                        ActionItem {
                            title: qsTr("复制凭据")
                            enabled: accountPage.hasCurrent
                            onTriggered: {
                                accountPage.copyCredential(accountPage.currentId)
                            }
                            imageSource: "asset:///icons/ic_copy.png"
                        },

                        ActionItem {
                            title: qsTr("删除账号")
                            enabled: accountPage.hasCurrent
                            onTriggered: {
                                accountPage.removeAccount(accountPage.currentId)
                            }
                            imageSource: "asset:///icons/ic_delete.png"
                        }
                    ]
                }
            ]

            ImageView {
                preferredWidth: ui.sdu(8)
                preferredHeight: ui.sdu(8)
                verticalAlignment: VerticalAlignment.Center
                scalingMethod: ScalingMethod.AspectFill
                imageSource: accountPage.hasCurrent
                             && music.currentAccount.avatarUrl
                             && music.currentAccount.avatarUrl.length > 0
                             ? music.imagePath(music.currentAccount.avatarUrl)
                             : "asset:///images/avatar_bright.png"
            }
            Container {
                leftPadding: ui.sdu(1.5)
                verticalAlignment: VerticalAlignment.Center
                layoutProperties: StackLayoutProperties {
                    spaceQuota: 1
                }
                layout: StackLayout {
                    orientation: LayoutOrientation.TopToBottom
                }
                Label {
                    text: accountPage.hasCurrent
                          ? music.currentAccount.nickName : qsTr("未登录")
                    textStyle {
                        base: SystemDefaults.TextStyles.PrimaryText
                    }
                }
                Label {
                    text: accountPage.hasCurrent
                          ? music.currentAccount.masked
                          : qsTr("登录后可同步歌单和听歌记录")
                    textStyle {
                        base: SystemDefaults.TextStyles.SmallText
                        color: ui.palette.textOnPlain
                    }
                }
            }
            Label {
                visible: accountPage.hasCurrent
                text: qsTr("当前")
                verticalAlignment: VerticalAlignment.Center
                textStyle {
                    base: SystemDefaults.TextStyles.SmallText
                    color: ui.palette.primary
                }
            }
        }

        // ===================== 已登录（其它已保存的账号） =====================
        Header {
            title: qsTr("已登录")
        }

        Container {
            visible: music.otherAccountCount == 0
            leftPadding: ui.sdu(2)
            rightPadding: ui.sdu(2)
            topPadding: ui.sdu(1)
         
        }

        ListView {
            id: otherList
            objectName: "accountListView"
            layoutProperties: StackLayoutProperties {
                spaceQuota: 1
            }
            dataModel: music.otherAccounts
            property variant controller: music

            listItemComponents: [
                ListItemComponent {
                    type: ""
                    Container {
                        id: accItem
                        leftPadding: ui.sdu(2)
                        rightPadding: ui.sdu(2)
                        topPadding: ui.sdu(1)
                        bottomPadding: ui.sdu(1)
                        layout: StackLayout {
                            orientation: LayoutOrientation.LeftToRight
                        }
                        contextActions: [
                            ActionSet {
                                title: ListItemData.nickName
                                subtitle: ListItemData.masked
                                actions: [
                                    ActionItem {
                                        title: qsTr("复制凭据")
                                        onTriggered: {
                                            // ★ 委托里调不到页面的函数（独立上下文，
                                            //   和看不到 music 是一个道理），全部走 controller
                                            var c = accItem.ListItem.view.controller
                                            var text = c.accountCredential(String(ListItemData.id))
                                            if (text && text.length > 0) {
                                                c.copyToClipboard(text)
                                                c.notifyError(qsTr("已复制凭据"))
                                            } else {
                                                c.notifyError(qsTr("失败"))
                                            }
                                        }
                                        imageSource: "asset:///icons/ic_copy.png"
                                    },
                                    ActionItem {
                                        // 不是当前账号 → 登录（切过去）
                                        title: qsTr("登录此账号")
                                        onTriggered: {
                                            var c = accItem.ListItem.view.controller
                                            if (c.switchAccount(String(ListItemData.id)))
                                                c.notifyError(qsTr("正在切换到 %1…")
                                                              .arg(ListItemData.nickName))
                                        }
                                        imageSource: "asset:///icons/ic_groups_white.png"
                                    },
                                    ActionItem {
                                        title: qsTr("删除账号")
                                        onTriggered: {
                                            var c = accItem.ListItem.view.controller
                                            c.removeAccount(String(ListItemData.id))
                                            c.notifyError(qsTr("已删除该账号"))
                                        }
                                        imageSource: "asset:///icons/ic_remove_friend.png"
                                    }
                                ]
                            }
                        ]

                        ImageView {
                            preferredWidth: ui.sdu(8)
                            preferredHeight: ui.sdu(8)
                            verticalAlignment: VerticalAlignment.Center
                            scalingMethod: ScalingMethod.AspectFill
                            imageSource: ListItemData.localAvatarPath
                                         && ListItemData.localAvatarPath.length > 0
                                         ? ListItemData.localAvatarPath
                                         : "asset:///images/avatar_bright.png"
                        }
                        Container {
                            leftPadding: ui.sdu(1.5)
                            verticalAlignment: VerticalAlignment.Center
                            layoutProperties: StackLayoutProperties {
                                spaceQuota: 1
                            }
                            layout: StackLayout {
                                orientation: LayoutOrientation.TopToBottom
                            }
                            Label {
                                text: ListItemData.nickName
                                textStyle {
                                    base: SystemDefaults.TextStyles.PrimaryText
                                }
                            }
                            Label {
                                text: ListItemData.masked
                                textStyle {
                                    base: SystemDefaults.TextStyles.SmallText
                                    color: ui.palette.textOnPlain
                                }
                            }
                        }
                    }
                }
            ]

            // 点一下 = 切到那个账号（和长按「登录此账号」等价）
            onTriggered: {
                var chosen = dataModel.data(indexPath)
                if (!chosen)
                    return
                if (music.switchAccount(String(chosen.id)))
                    music.notifyError(qsTr("正在切换到 %1…").arg(chosen.nickName))
            }
        }
    }

    /*!
     * 复制某账号的 MUSIC_U 全文。
     * ★ 明文只在 C++ → 剪贴板之间过一手，不进模型、不打印。
     */
    function copyCredential(id) {
        var text = music.accountCredential(id)
        if (!text || text.length == 0) {
            music.notifyError(qsTr("失败"))
            return
        }
        music.copyToClipboard(text)
        music.notifyError(qsTr("已复制凭据"))
    }

    /*! 删除账号（带二次确认的提示语，删除不可逆所以先问一句） */
    function removeAccount(id) {
        music.removeAccount(id)
        music.notifyError(qsTr("已删除该账号"))
    }
}
