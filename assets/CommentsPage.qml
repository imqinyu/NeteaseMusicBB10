// CommentsPage - 评论页（由 NavigationPane push 进来）
//
// 返回不用自己做按钮：BB10 的 NavigationPane 自带返回键 + 左边缘滑动返回，
// 这是平台原生的导航方式（ModPlayer 也是每个详情页这么推）。
//
// 用法（主界面里）：
//     var page = commentsPageDef.createObject(root)
//     root.activePane.push(page)

import bb.cascades 1.3

Page {
    Menu.definition: MenuDefinition {
        actions: [
            ActionItem {
                title: "正在播放"
                imageSource: "asset:///icons/ic_play_on.png"
                onTriggered: {
                    music.requestOpenNowPlaying()
                }
            }
        ]
    }
    id: commentsPage
    objectName: "commentsPage"

    titleBar: TitleBar {
        title: (music.commentsTitle.length > 0 ? music.commentsTitle : qsTr("评论"))
              + (music.commentTotal > 0 ? " (" + music.commentTotal + ")" : "")
    }

    Container {
        layout: StackLayout {
            orientation: LayoutOrientation.TopToBottom
        }

        // ---- 加载中（参照 BBTieba：加载态单独占一行，别和旧内容混在一起） ----
        Container {
            visible: music.loading
            leftPadding: 16
            rightPadding: 16
            topPadding: 14
            bottomPadding: 10
            layout: StackLayout {
                orientation: LayoutOrientation.LeftToRight
            }
            ActivityIndicator {
                running: true
                preferredWidth: 40
                preferredHeight: 40
                verticalAlignment: VerticalAlignment.Center
            }
            Label {
                text: qsTr("正在加载评论…")
                verticalAlignment: VerticalAlignment.Center
                textStyle {
                    base: SystemDefaults.TextStyles.BodyText
                }
            }
        }

        // ---- 空状态 ----
        // ★ 用「已加载条数」判断，而不是 commentTotal：
        //   有些歌曲 commentTotal 为 0 但热评已经加载出来，会误显示"没评论"
        Container {
            visible: (!music.loading) && music.commentsVersion > 0
                     && music.commentCount() == 0
            leftPadding: 16
            rightPadding: 16
            topPadding: 20
            Label {
                text: qsTr("这首歌还没有评论")
                textStyle {
                    base: SystemDefaults.TextStyles.BodyText
                    color: ui.palette.textOnPlain
                }
            }
        }

        ListView {
            layoutProperties: StackLayoutProperties {
                spaceQuota: 1
            }
            dataModel: music.comments
            // ★ 委托里看不到 context property `music`，长按菜单要走 ListItem.view.controller
            property variant controller: music
            listItemComponents: [
                ListItemComponent {
                    type: ""
                    Container {
                        id: commentItem
                        contextActions: [
                            ActionSet {
                                title: ListItemData.userName
                                subtitle: ListItemData.content
                                actions: [
                                    ActionItem {
                                        title: qsTr("复制")
                                        onTriggered: {
                                            var c = commentItem.ListItem.view.controller
                                            c.copyToClipboard(ListItemData.content)
                                            c.notifyError(qsTr("已复制评论内容"))
                                        }
                                        imageSource: "asset:///icons/ic_copy.png"
                                    },
                                    ActionItem {
                                        title: qsTr("打开用户资料")
                                        // 早期缓存里的评论没存 userId，那种就点不了。
                                        // ★ 用 Number() 判：undefined 会变成 NaN，比 String() 可靠
                                        enabled: Number(ListItemData.userId) > 0
                                        onTriggered: {
                                            // 昵称/头像一起带过去，资料页不用等接口就能显示
                                            commentItem.ListItem.view.controller.requestOpenUser(
                                                String(ListItemData.userId),
                                                ListItemData.userName,
                                                ListItemData.userAvatarUrl)
                                        }
                                        imageSource: "asset:///icons/ic_open_contacts.png"
                                    }
                                ]
                            }
                        ]
                        leftPadding: 12
                        rightPadding: 12
                        topPadding: 8
                        bottomPadding: 8
                        layout: StackLayout {
                            orientation: LayoutOrientation.LeftToRight
                        }
                        ImageView {
                            preferredWidth: 64
                            preferredHeight: 64
                            imageSource: ListItemData.localAvatarPath
                                   ? ListItemData.localAvatarPath
                                   : "asset:///images/avatar_bright.png"
                            scalingMethod: ScalingMethod.AspectFit
                        }
                        Container {
                            leftPadding: 12
                            layoutProperties: StackLayoutProperties {
                                spaceQuota: 1
                            }
                            layout: StackLayout {
                                orientation: LayoutOrientation.TopToBottom
                            }
                            Container {
                                layout: StackLayout {
                                    orientation: LayoutOrientation.LeftToRight
                                }
                                Label {
                                    text: ListItemData.userName
                                    layoutProperties: StackLayoutProperties {
                                        spaceQuota: 1
                                    }
                                    textStyle {
                                        base: SystemDefaults.TextStyles.PrimaryText
                                    }
                                }
                                Label {
                                    text: qsTr("热评")
                                    visible: ListItemData.isHot
                                    textStyle {
                                        base: SystemDefaults.TextStyles.SmallText
                                        color: Color.create("#ff6b6b")
                                    }
                                }
                                Label {
                                    text: qsTr("%1 赞").arg(ListItemData.likedCount)
                                    textStyle {
                                        base: SystemDefaults.TextStyles.SmallText
                                        color: ui.palette.textOnPlain
                                    }
                                }
                            }
                            Label {
                                text: ListItemData.content
                                multiline: true
                                textStyle {
                                    base: SystemDefaults.TextStyles.BodyText
                                }
                            }
                            Label {
                                text: ListItemData.timeText
                                textStyle {
                                    base: SystemDefaults.TextStyles.SmallText
                                    color: ui.palette.textOnPlain
                                }
                            }
                        }
                    }
                }
            ]
            attachedObjects: [
                ListScrollStateHandler {
                    onScrollingChanged: {
                        music.setScrolling(scrolling)
                    }
                }
            ]
        }
    }
}
