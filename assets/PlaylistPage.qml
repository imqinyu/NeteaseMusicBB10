// PlaylistPage - 歌单 / 专辑 / 艺人搜索结果 共用的「曲目列表页」
//
// 设计来自 NMDemoUI/assets/PlaylistPage.qml 的注释：
//   布局参考黑莓播放器的专辑页，音乐列表可以用 header 做字母/分组分割。
// 顶部是歌单封面 + 标题，下面是曲目列表；动作栏：播放 / 收藏 / 评论 / 搜索。
//
// 数据：music.browseSongs（浏览模型）。歌单 / 专辑 / 搜索结果都进这里，
//       【不动播放队列 music.songs】；点某首播放时才由 playBrowseSong 拷成队列。

import bb.cascades 1.4
import bb.multimedia 1.0

Page {
    Menu.definition: MenuDefinition {
        actions: [
            ActionItem {
                title: qsTr("正在播放") + Retranslate.onLocaleOrLanguageChanged
                imageSource: "asset:///icons/ic_play_on.png"
                onTriggered: {
                    music.requestOpenNowPlaying()
                }
            }
        ]
    }
    id: playlistPage

    /*
     * ★ 不再需要 restoreListOnPop：本页现在用独立浏览模型，
     *   从头到尾都没碰过播放队列 m_songs，本来就不需要还原。
     *   （保留 m_queueAdopted 那套逻辑给艺人页用。）
     */
    objectName: "playlistPage"

    titleBar: TitleBar {
        title: music.listTitle.length > 0 ? music.listTitle : qsTr("曲目") + Retranslate.onLocaleOrLanguageChanged
    }

    // 供 main.qml 的 pop 处理在销毁本页前清空列表（缓解返回卡顿）。
    // 非歌单页没有这个属性，main 里用 `page.myList` 判断即可，不会报错。
    property alias myList: listView

    actions: [
        ActionItem {
            title: qsTr("播放") + Retranslate.onLocaleOrLanguageChanged
            imageSource: "asset:///icons/ic_play.png"
            ActionBar.placement: ActionBarPlacement.Signature
            onTriggered: {
                // ★ 播的是浏览列表第一首；songCount() 是队列数，这里要用 browseCount()
                if (music.browseCount() > 0)
                    music.playBrowseSong(0)
            }
        },
        ActionItem {
            title: qsTr("收藏") + Retranslate.onLocaleOrLanguageChanged
            imageSource: "asset:///icons/ic_favorite.png"
            ActionBar.placement: ActionBarPlacement.OnBar
            onTriggered: {
                music.notifyError(qsTr("收藏功能还没接后端") + Retranslate.onLocaleOrLanguageChanged)
            }
        },
        ActionItem {
            title: qsTr("评论") + Retranslate.onLocaleOrLanguageChanged
            imageSource: "asset:///icons/ic_chat_multiperson.png"
            ActionBar.placement: ActionBarPlacement.Default
            onTriggered: {
                // 歌单页要看的是【歌单自己的评论】，用这个而不是
                // requestCommentsForCurrent（那个需要正在播放某首歌）
                music.requestPlaylistComments()
            }
        },
        ActionItem {
            title: qsTr("搜索") + Retranslate.onLocaleOrLanguageChanged
            imageSource: "asset:///icons/ic_search.png"
            ActionBar.placement: ActionBarPlacement.OnBar
            onTriggered: {
                playlistPage.searchVisible = !playlistPage.searchVisible
                if (playlistPage.searchVisible)
                    innerSearchField.requestFocus()
                else
                    innerSearchField.text = ""
            }
        }
    ]

    property bool searchVisible: false

    /*! 图片缓存版本：封面下完之后靠它让绑定重算（见下面头部封面的说明） */
    property int artVersion: music.imageCacheVersion

    Container {
        layout: StackLayout {
            orientation: LayoutOrientation.TopToBottom
        }

        // ---- 歌单/专辑头（封面 + 名称 + 曲目数；仿官方专辑页/歌单页）----
        // 搜索结果没有这些信息，playlistInfo 为空时自动隐藏。
        // ★ 黑莓屏幕小：封面和字号都收小，标题允许折行但不撑高。
        Container {
            visible: music.playlistInfo && music.playlistInfo.name
                     && music.playlistInfo.name.length > 0
            horizontalAlignment: HorizontalAlignment.Fill
            leftPadding: ui.sdu(1.5)
            rightPadding: ui.sdu(1.5)
            topPadding: ui.sdu(1)
            bottomPadding: ui.sdu(0.5)
            layout: StackLayout {
                orientation: LayoutOrientation.LeftToRight
            }
            ImageView {
                preferredWidth: ui.sdu(12)
                preferredHeight: ui.sdu(12)
                verticalAlignment: VerticalAlignment.Center
                scalingMethod: ScalingMethod.AspectFill
                // ★ 必须挂 artVersion（= imageCacheVersion）这个依赖：
                //   封面是异步下载的，下完之后若没有依赖变化，绑定不会重算 ——
                //   表现就是"头部封面永远是占位图"（真机反馈）。
                imageSource: (playlistPage.artVersion >= 0
                              && music.playlistInfo.coverUrl
                              && music.playlistInfo.coverUrl.length > 0
                              && music.imagePath(music.playlistInfo.coverUrl).length > 0)
                             ? music.imagePath(music.playlistInfo.coverUrl)
                             : "asset:///images/ic_default.png"
            }
            Container {
                leftPadding: ui.sdu(1.5)
                verticalAlignment: VerticalAlignment.Center
                layout: StackLayout {
                    orientation: LayoutOrientation.TopToBottom
                }
                Label {
                    text: music.playlistInfo.name ? music.playlistInfo.name : ""
                    multiline: true
                    textStyle {
                        base: SystemDefaults.TextStyles.SubtitleText
                        fontWeight: FontWeight.Bold
                    }
                }
                Label {
                    // 专辑显示发行日期、歌单显示创建日期（dateText 由 C++ 格式化成
                    // 「2025年4月27日」；无日期时为空，表现为直接「48首」）。
                    // ★ 用 songTotal（带 NOTIFY 的属性），不是 songCount()：
                    //   后者是 Q_INVOKABLE，写进绑定只算一次 → 永远"0 首"
                    text: (music.playlistInfo && music.playlistInfo.dateText
                           && music.playlistInfo.dateText.length > 0)
                          ? music.playlistInfo.dateText + "  " + qsTr("%1 首").arg(music.browseTotal)
                          : qsTr("%1 首").arg(music.browseTotal)
                    textStyle {
                        base: SystemDefaults.TextStyles.SmallText
                        color: ui.palette.textOnPlain
                    }
                }
            }
        }

        // ---- 歌单内搜索（按上面「搜索」动作切换显隐） ----
        Container {
            visible: playlistPage.searchVisible
            leftPadding: ui.sdu(2)
            rightPadding: ui.sdu(2)
            topPadding: ui.sdu(1)
            bottomPadding: ui.sdu(1)
            TextField {
                id: innerSearchField
                hintText: qsTr("在当前列表里过滤") + Retranslate.onLocaleOrLanguageChanged
                input {
                    submitKey: SubmitKey.Search
                }
                onTextChanging: {
                    music.setListFilter(text)
                }
            }
        }

        // ---- 加载中 ----
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
                text: qsTr("正在加载…") + Retranslate.onLocaleOrLanguageChanged
                verticalAlignment: VerticalAlignment.Center
                textStyle {
                    base: SystemDefaults.TextStyles.SmallText
                }
            }
        }

        // ---- 空状态 ----
        Container {
            visible: (!music.loading) && music.browseCount() == 0
            leftPadding: ui.sdu(2)
            rightPadding: ui.sdu(2)
            topPadding: ui.sdu(4)
            Label {
                text: qsTr("这个列表是空的") + Retranslate.onLocaleOrLanguageChanged
                textStyle {
                    base: SystemDefaults.TextStyles.BodyText
                    color: ui.palette.textOnPlain
                }
            }
        }

        // ---- 曲目列表 ----
        ListView {
            id: listView
            objectName: "ppListView"
            layoutProperties: StackLayoutProperties {
                spaceQuota: 1
            }
            /*
             * ★ 用【浏览模型】 browseSongs，不是 music.songs（播放队列）。
             *   打开歌单 / 专辑 / 搜索结果只是"看看"，不该动播放队列；
             *   只有点某首（playBrowseSong）才把这份列表拷贝成队列。
             */
            dataModel: music.browseSongs
            property variant controller: music

            listItemComponents: [
                ListItemComponent {
                    // ArrayDataModel::itemType() 恒为 ""，只能用 type:""
                    type: ""
                    Container {
                        id: songItem
                        contextActions: [
                            ActionSet {
                                title: ListItemData.name
                                subtitle: ListItemData.artistsText
                                actions: [
                                    ActionItem {
                                        title: qsTr("播放") + Retranslate.onLocaleOrLanguageChanged
                                        onTriggered: {
                                            songItem.ListItem.view.controller.playBrowseSong(songItem.ListItem.indexPath[0])
                                        }
                                        imageSource: "asset:///icons/ic_play.png"
                                    },
                                    ActionItem {
                                        
                                        // 把这首歌插到当前曲目的下一位（见 MusicController::playNextSong）
                                        title: qsTr("下一首播放") + Retranslate.onLocaleOrLanguageChanged
                                        onTriggered: {
                                            var c = songItem.ListItem.view.controller
                                            var ok = c.playNextSong(ListItemData)
                                            c.notifyError(ok
                                                          ? qsTr("已加入下一首：%1").arg(ListItemData.name)
                                                          : qsTr("这首歌已经在下一首了") + Retranslate.onLocaleOrLanguageChanged)
                                        }
                                        imageSource: "asset:///icons/ic_add_to_play_next.png"
                                    },
                                    ActionItem {
                                        title: qsTr("查看评论") + Retranslate.onLocaleOrLanguageChanged
                                        onTriggered: {
                                            songItem.ListItem.view.controller.requestCommentsForBrowseIndex(songItem.ListItem.indexPath[0])
                                        }
                                        imageSource: "asset:///icons/ic_chat_multiperson.png"
                                    },
                                    ActionItem {
                                        
                                        title: qsTr("播放 MV") + Retranslate.onLocaleOrLanguageChanged
                                        enabled: ListItemData.hasMv === true
                                        onTriggered: {
                                            songItem.ListItem.view.controller.openMv(
                                                ListItemData.mvId,
                                                ListItemData.name,
                                                ListItemData.artistsText)
                                        }
                                        imageSource: "asset:///icons/GridIconVideo.png"
                                    },
                                    ActionItem {
                                        // 音乐详情（Properties）：看这一首的信息 / 歌词
                                        title: qsTr("音乐详情") + Retranslate.onLocaleOrLanguageChanged
                                        onTriggered: {
                                            songItem.ListItem.view.controller.requestOpenPropertiesFor(ListItemData)
                                        }
                                        imageSource: "asset:///icons/ic_info.png"
                                    }
                                ]
                            }
                        ]
                        SongListItem {
                            title: ListItemData.name
                            subtitle: ListItemData.artistsText
                                      + (ListItemData.albumName.length > 0
                                         ? " · " + ListItemData.albumName : "")
                            imageSource: ListItemData.localArtPath ? ListItemData.localArtPath : ""
                            durationText: ListItemData.durationText
                            isVip: ListItemData.isVip
                            hasMv: ListItemData.hasMv === true
                        }
                    }
                }
            ]

            onTriggered: {
                // ★ 这一步才把浏览列表变成播放队列并开始播
                music.playBrowseSong(indexPath[0])
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
