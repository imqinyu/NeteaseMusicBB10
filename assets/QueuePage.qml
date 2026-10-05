// QueuePage - 播放队列
//
// 入口：正在播放页长按 → 「播放队列」（music.requestOpenQueue）。
//
// ★ 本项目的「播放队列」就是 music.songs 本身：
//     next()/prev() 取的是 (currentIndex ± 1) % songs.size()，
//     「下一首播放」= 往 songs 里 currentIndex 后面插一项。
//   所以这一页不新增任何数据源，只是把 songs 列出来 + 标出正在播的那首。
//
// ⚠️ 和 PlaylistPage 一样：ListItemComponent 里看不到 context property `music`，
//    必须走 `ListItem.view.controller`（ListView 上挂了 property variant controller: music）。

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
    id: queuePage
    objectName: "queuePage"

    titleBar: TitleBar {
        title: qsTr("播放队列")
    }

    // 供 main.qml 的 pop 处理在销毁本页前把 ListView 从模型上摘下来
    // （让 delegate 立即释放，缓解返回卡顿）。只摘引用，不动 music.songs 本身。
    property alias myList: listView

    actions: [
        ActionItem {
            // 清空队列：只清列表、不停播（见 MusicController::clearQueue）
            title: qsTr("清空")
            imageSource: "asset:///icons/ic_delete.png"
            ActionBar.placement: ActionBarPlacement.OnBar
            enabled: music.songCount() > 0
            onTriggered: {
                music.clearQueue()
                music.notifyError(qsTr("播放队列已清空"))
            }
        }
    ]

    Container {
        layout: StackLayout {
            orientation: LayoutOrientation.TopToBottom
        }

        // ---- 空状态 ----
        Container {
            visible: music.songCount() == 0
            leftPadding: ui.sdu(2)
            rightPadding: ui.sdu(2)
            topPadding: ui.sdu(4)
            Label {
                text: qsTr("播放队列是空的")
                textStyle {
                    base: SystemDefaults.TextStyles.BodyText
                    color: ui.palette.textOnPlain
                }
            }
        }

        // ---- 队列列表 ----
        ListView {
            id: listView
            objectName: "queueListView"
            layoutProperties: StackLayoutProperties {
                spaceQuota: 1
            }
            dataModel: music.songs
            property variant controller: music

            listItemComponents: [
                ListItemComponent {
                    // ArrayDataModel::itemType() 恒为 ""，只能用 type:""
                    type: ""
                    Container {
                        id: queueItem
                        contextActions: [
                            ActionSet {
                                title: ListItemData.name
                                subtitle: ListItemData.artistsText
                                actions: [
                                    ActionItem {
                                        title: qsTr("播放")
                                        onTriggered: {
                                            queueItem.ListItem.view.controller.playIndex(queueItem.ListItem.indexPath[0])
                                        }
                                        imageSource: "asset:///icons/ic_play.png"
                                    },
                                    ActionItem {
                                        title: qsTr("下一首播放")
                                        onTriggered: {
                                            var c = queueItem.ListItem.view.controller
                                            var ok = c.playNextSong(ListItemData)
                                            c.notifyError(ok
                                                          ? qsTr("已加入下一首：%1").arg(ListItemData.name)
                                                          : qsTr("这首歌已经在下一首了"))
                                        }
                                        imageSource: "asset:///icons/ic_add_to_play_next.png"
                                    },
                                    ActionItem {
                                        title: qsTr("查看评论")
                                        onTriggered: {
                                            queueItem.ListItem.view.controller.requestCommentsForIndex(queueItem.ListItem.indexPath[0])
                                        }
                                        imageSource: "asset:///icons/ic_chat_multiperson.png"
                                    },
                                    ActionItem {
                                        title: qsTr("播放 MV")
                                        enabled: ListItemData.hasMv === true
                                        onTriggered: {
                                            queueItem.ListItem.view.controller.openMv(
                                                ListItemData.mvId,
                                                ListItemData.name,
                                                ListItemData.artistsText)
                                        }
                                        imageSource: "asset:///icons/GridIconVideo.png"
                                    },
                                    ActionItem {
                                        // 从队列里移除这一首（见 MusicController::removeSongAt）
                                        title: qsTr("删除")
                                        onTriggered: {
                                            var c = queueItem.ListItem.view.controller
                                            c.removeSongAt(queueItem.ListItem.indexPath[0])
                                            c.notifyError(qsTr("已从队列移除：%1").arg(ListItemData.name))
                                        }
                                        imageSource: "asset:///icons/ic_delete.png"
                                    },
                                    ActionItem {
                                        // 音乐详情（Properties）：看这一首的信息 / 歌词
                                        title: qsTr("音乐详情")
                                        onTriggered: {
                                            queueItem.ListItem.view.controller.requestOpenPropertiesFor(ListItemData)
                                        }
                                        imageSource: "asset:///icons/ic_info.png"
                                    }
                                ]
                            }
                        ]
                        SongListItem {
                            title: ListItemData.name
                            subtitle: ListItemData.artistsText
                                      + ((ListItemData.albumName
                                          && ListItemData.albumName.length > 0)
                                         ? " · " + ListItemData.albumName : "")
                            imageSource: ListItemData.localArtPath ? ListItemData.localArtPath : ""
                            durationText: ListItemData.durationText
                            isVip: ListItemData.isVip
                            hasMv: ListItemData.hasMv === true
                            // 正在播的那首：加粗高亮（SongListItem 的 playing）
                            // ★ 照全部歌曲页的写法比 id，而不是比 indexPath：
                            //   indexPath 在绑定里不一定拿得到，比 id 更稳。
                            playing: String(ListItemData.id)
                                     === String(queueItem.ListItem.view.controller.currentTrack.id)
                        }
                    }
                }
            ]

            onTriggered: {
                music.playIndex(indexPath[0])
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
