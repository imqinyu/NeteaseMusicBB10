// SearchPage - 搜索页
//
// 交互照系统联系人应用（Contacts ui 的 ContactSearch）：
//   顶部搜索框，但【改为手动提交】—— 回车 / 点动作栏「搜索」才发请求。
//   ★ 原来边打字边发（满 2 字就发）会连打接口，容易被网易限流拉黑
//     （真机出现过 code=405、结果全空）。
// 结果列表用 SongListItem + 长按菜单（播放 / 查看评论）。

import bb.cascades 1.4

Page {
    id: searchPage
    objectName: "searchPage"

    titleBar: TitleBar {
        title: qsTr("搜索")
    }

    actions: [
        ActionItem {
            /*
             * ★ 手动提交搜索：不再边打字边发请求（高频会被服务端限流拉黑）。
             *   键盘上的回车（onSubmitted）和这个按钮走同一条路。
             */
            title: qsTr("搜索")
            imageSource: "asset:///icons/ic_search.png"
            ActionBar.placement: ActionBarPlacement.OnBar
            onTriggered: {
                music.search(searchField.text)
            }
        }
    ]

    Container {
        layout: StackLayout {
            orientation: LayoutOrientation.TopToBottom
        }

        Container {
            leftPadding: ui.sdu(2)
            rightPadding: ui.sdu(2)
            topPadding: ui.sdu(1)
            bottomPadding: ui.sdu(1)
            TextField {
                id: searchField
                hintText: qsTr("搜索歌曲 / 歌手")
                inputMode: TextFieldInputMode.Text
                input {
                    submitKey: SubmitKey.Search
                    onSubmitted: {
                        music.search(searchField.text)
                    }
                }
                // ★ 不再 onTextChanging 自动搜索：改由回车 / 动作栏「搜索」手动提交
            }
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
                text: qsTr("正在搜索…")
                verticalAlignment: VerticalAlignment.Center
                textStyle {
                    base: SystemDefaults.TextStyles.SmallText
                }
            }
        }

        Container {
            /*
             * ★ 用 browseTotal（浏览列表曲目数，带 NOTIFY）而不是 songCount()：
             *   搜索结果现在填在【浏览列表】里，songCount() 是播放队列的数，
             *   永远对不上 → 这条"没找到"就会一直挂着（真机反馈）。
             */
            visible: (!music.loading) && music.browseTotal == 0
                     && searchField.text.length > 0
            leftPadding: ui.sdu(2)
            rightPadding: ui.sdu(2)
            topPadding: ui.sdu(3)
            Label {
                text: qsTr("没有找到相关歌曲")
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
            // ★ 用搜索页【独立】模型：全局 music.songs 会被"播放任意一首歌"时的
            //   setPlaylistFrom() 换成那首歌所在的歌单/专辑，绑它就会显示成
            //   "当前歌相关的列表"（真机反馈）
            dataModel: music.searchSongs
            property variant controller: music

            listItemComponents: [
                ListItemComponent {
                    // ArrayDataModel::itemType() 恒返回 ""，所以只能用 type:""
                    type: ""
                    Container {
                        id: item
                        contextActions: [
                            ActionSet {
                                title: ListItemData.name
                                subtitle: ListItemData.artistsText
                                actions: [
                                    ActionItem {
                                        title: qsTr("播放")
                                        onTriggered: {
                                            /*
                                             * ★ 用 playSearchSong（按【搜索页独立模型】的下标取数）。
                                             *   不能用 playBrowseSong —— 搜索页显示的是"搜索那一刻
                                             *   的快照"，之后打开歌单/专辑会把浏览列表换掉，
                                             *   按 browse 下标取就会播成不相干的歌（真机反馈）。
                                             */
                                            item.ListItem.view.controller.playSearchSong(item.ListItem.indexPath[0])
                                        }
                                    },
                                    ActionItem {
                                        // 把这首歌插到当前曲目的下一位（见 MusicController::playNextSong）
                                        title: qsTr("下一首播放")
                                        onTriggered: {
                                            var c = item.ListItem.view.controller
                                            var ok = c.playNextSong(ListItemData)
                                            c.notifyError(ok
                                                          ? qsTr("已加入下一首：%1").arg(ListItemData.name)
                                                          : qsTr("这首歌已经在下一首了"))
                                        }
                                    },
                                    ActionItem {
                                        title: qsTr("查看评论")
                                        onTriggered: {
                                            // ★ 同上：按【搜索页独立模型】的下标取评论
                                            item.ListItem.view.controller.requestCommentsForSearchIndex(item.ListItem.indexPath[0])
                                        }
                                    },
                                    ActionItem {
                                        title: qsTr("播放 MV")
                                        enabled: ListItemData.hasMv === true
                                        onTriggered: {
                                            item.ListItem.view.controller.openMv(
                                                ListItemData.mvId,
                                                ListItemData.name,
                                                ListItemData.artistsText)
                                        }
                                    },
                                    ActionItem {
                                        /*
                                         * 音乐详情（Properties）：把这一项本身交给详情页。
                                         * ★ 详情页有自己的歌词通道（m_propertiesLyric），
                                         *   看别的歌不会顶掉正在播放那首的歌词。
                                         */
                                        title: qsTr("音乐详情")
                                        onTriggered: {
                                            item.ListItem.view.controller.requestOpenPropertiesFor(ListItemData)
                                        }
                                        imageSource: "asset:///icons/ic_info.png"
                                    }
                                ]
                            }
                        ]
                        SongListItem {
                            title: ListItemData.name
                            subtitle: ListItemData.artistsText
                            imageSource: ListItemData.localArtPath ? ListItemData.localArtPath : ""
                            durationText: ListItemData.durationText
                            isVip: ListItemData.isVip
                            hasMv: ListItemData.hasMv === true
                        }
                    }
                }
            ]

            onTriggered: {
                var chosen = dataModel.data(indexPath)
                // ★ 播的是搜索结果（搜索页独立模型）里的这一首，不是队列下标
                if (chosen)
                    music.playSearchSong(indexPath[0])
            }

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
