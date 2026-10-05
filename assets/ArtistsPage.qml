// ArtistsPage - 关注的艺人列表页
// 数据来自 music.artists（/api/artist/sublist，登录后自动拉取）

import bb.cascades 1.4

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
    id: artistsPage
    objectName: "artistsPage"

    titleBar: TitleBar {
        title: qsTr("关注的艺人")
    }

    Container {
        layout: StackLayout {
            orientation: LayoutOrientation.TopToBottom
        }

        Container {
            visible: music.loading
            leftPadding: ui.sdu(2)
            rightPadding: ui.sdu(2)
            layout: StackLayout {
                orientation: LayoutOrientation.LeftToRight
            }
            ActivityIndicator {
                running: true
                preferredWidth: ui.sdu(6)
                preferredHeight: ui.sdu(6)
                verticalAlignment: VerticalAlignment.Center
            }
            Label {
                text: qsTr("正在加载…")
                verticalAlignment: VerticalAlignment.Center
                textStyle {
                    base: SystemDefaults.TextStyles.SmallText
                }
            }
        }

        Container {
            // ★ 之前漏了数量判断：只要拉过一次（artistsVersion>0）就显示
            //   "还没有关注艺人"，哪怕下面已经有艺人了。
            visible: (!music.loading) && music.artistsVersion > 0
                     && music.artistCount() == 0
            leftPadding: ui.sdu(2)
            rightPadding: ui.sdu(2)
            topPadding: ui.sdu(3)
            Label {
                text: qsTr("还没有关注艺人")
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
            dataModel: music.artists
            property variant controller: music

            listItemComponents: [
                ListItemComponent {
                    type: ""
                    Container {
                        id: item
                        contextActions: [
                            ActionSet {
                                title: ListItemData.name
                                subtitle: ListItemData.trans
                                actions: [
                                    ActionItem {
                                        title: qsTr("搜索该艺人")
                                        onTriggered: {
                                            item.ListItem.view.controller.search(item.ListItem.data.name)
                                        }
                                    }
                                ]
                            }
                        ]
                        ArtistListItem {
                            title: ListItemData.name
                            subtitle: qsTr("%1 张专辑").arg(ListItemData.albumSize)
                            imageSource: ListItemData.localPicPath ? ListItemData.localPicPath : ""
                        }
                    }
                }
            ]

            onTriggered: {
                // 点条目直接进艺人资料页（不再只加载歌曲、也不污染全局列表）
                var chosen = dataModel.data(indexPath)
                if (chosen)
                    music.requestOpenArtist(chosen.name)
            }
        }
    }

    actions: [
        ActionItem {
            title: qsTr("刷新")
            imageSource: "asset:///icons/ic_reload.png"
            onTriggered: {
                music.loadMyProfile()
            }
        }
    ]
}
