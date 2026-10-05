// ArtistDetailPage - 艺人资料页
//
// 版式照 UserDetailPage（个人主页）：顶部「背景图 + 头像 + 名字/统计」，
// 下面 SegmentedControl 切 热门 / 专辑。
//
// ★ 数据走【独立模型】music.artistSongs / music.artistAlbums（由
//   music.loadArtist(名字) 填充）—— 绝不动全局 music.songs/albums，
//   否则推荐页的「歌曲」会变成这个艺人的歌（真机反馈）。
// ★ artistName 是字符串（main 用 extraSetup 注入），不要注入函数。

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
    id: artistPage
    objectName: "artistDetailPage"

    // main.qml 注入
    property string artistName

    property int artVersion: music.imageCacheVersion
    // 艺人资料（picUrl / albumSize / mvSize）
    property variant info: music.artistInfo(artistName)
    property string picUrl: (info && info.picUrl) ? info.picUrl : ""

    // 头部：照通讯录联系人详情页——默认缩小，点一下放大，再点缩回；
    // 列表滚动时整块收起，只留分段标签
    property bool headerCollapsed: false
    property bool headerExpanded: false

    // 进页面就拉这个艺人的歌（进独立模型）
    onArtistNameChanged: {
        if (artistName.length > 0)
            music.loadArtist(artistName)
    }

//    titleBar: TitleBar {
//        title: artistName.length > 0 ? artistName : qsTr("艺人")
//    }


    Container {
        horizontalAlignment: HorizontalAlignment.Fill
        verticalAlignment: VerticalAlignment.Fill
        layout: StackLayout {
            orientation: LayoutOrientation.TopToBottom
        }

        // ---- 顶部：背景图 + 头像 + 名字/统计 ----
        // 默认缩小，点一下放大（通讯录联系人详情页同款）；滚动时整块收起
        Container {
            id: artistHeader
            horizontalAlignment: HorizontalAlignment.Fill
            preferredHeight: artistPage.headerExpanded ? ui.sdu(56) : ui.sdu(19)
            layout: DockLayout {
            }
            gestureHandlers: [
                TapHandler {
                    onTapped: artistPage.headerExpanded = !artistPage.headerExpanded
                }
            ]

            ImageView {
                horizontalAlignment: HorizontalAlignment.Fill
                verticalAlignment: VerticalAlignment.Fill
                scalingMethod: ScalingMethod.AspectFill
                opacity: 0.5
                imageSource: artistPage.artVersion >= 0 && artistPage.picUrl.length > 0
                              && music.imagePath(artistPage.picUrl).length > 0
                             ? music.imagePath(artistPage.picUrl)
                             : "asset:///images/ic_default.png"
            }

            Container {
                horizontalAlignment: HorizontalAlignment.Left
                verticalAlignment: VerticalAlignment.Bottom
                leftPadding: ui.sdu(3)
                bottomPadding: ui.sdu(2)
                layout: StackLayout {
                    orientation: LayoutOrientation.LeftToRight
                }

                ImageView {
                    // 默认 14du 的缩略头像，放大时到 30du
                    preferredWidth: artistPage.headerExpanded ? ui.sdu(30) : ui.sdu(14)
                    preferredHeight: artistPage.headerExpanded ? ui.sdu(30) : ui.sdu(14)
                    verticalAlignment: VerticalAlignment.Center
                    scalingMethod: ScalingMethod.AspectFill
                    imageSource: artistPage.artVersion >= 0 && artistPage.picUrl.length > 0
                                  && music.imagePath(artistPage.picUrl).length > 0
                                 ? music.imagePath(artistPage.picUrl)
                                 : "asset:///images/avatar_bright.png"
                }

                Container {
                    leftPadding: ui.sdu(2)
                    verticalAlignment: VerticalAlignment.Center
                    layout: StackLayout {
                        orientation: LayoutOrientation.TopToBottom
                    }
                    Label {
                        text: artistPage.artistName
                        textStyle {
                            base: SystemDefaults.TextStyles.TitleText
                        }
                    }
                    Label {
                        // 缩小时只留头像 + 名字，统计这行藏起来
                        visible: artistPage.headerExpanded
                        text: artistPage.info
                              ? qsTr("专辑 %1 · MV %2")
                                .arg(artistPage.info.albumSize)
                                .arg(artistPage.info.mvSize)
                              : ""
                        textStyle {
                            base: SystemDefaults.TextStyles.SubtitleText
                            color: ui.palette.textOnPlain
                        }
                    }
                }
            }
        }

        // ---- 分段：热门 / 专辑 ----
        SegmentedControl {
            id: artistMode
            // 切分段时把头部放回来
            onSelectedValueChanged: artistPage.headerCollapsed = false
            Option {
                text: qsTr("热门")
                value: "hot"
                selected: true
            }
            Option {
                text: qsTr("专辑")
                value: "albums"
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
                text: qsTr("正在加载…")
                verticalAlignment: VerticalAlignment.Center
                textStyle {
                    base: SystemDefaults.TextStyles.SmallText
                }
            }
        }

        // ---- 热门（独立模型）----
        ListView {
            id: hotList
            visible: artistMode.selectedValue == "hot"
            layoutProperties: StackLayoutProperties {
                spaceQuota: 1
            }
            dataModel: music.artistSongs
            // ★ 委托（ListItemComponent）里看不到 context property `music`，
            //   长按菜单必须走 ListItem.view.controller（原来是 undefined，动作全是坏的）
            property variant controller: music
            listItemComponents: [
                ListItemComponent {
                    type: ""
                    Container {
                        id: artistSongItem
                        contextActions: [
                            ActionSet {
                                title: ListItemData.name
                                subtitle: ListItemData.artistsText
                                actions: [
                                    ActionItem {
                                        title: qsTr("播放")
                                        onTriggered: {
                                            artistSongItem.ListItem.view.controller.playArtistSong(artistSongItem.ListItem.indexPath[0])
                                        }
                                        imageSource: "asset:///icons/ic_play.png"
                                    },
                                    ActionItem {
                                        title: qsTr("播放 MV")
                                        enabled: ListItemData.hasMv === true
                                        onTriggered: {
                                            artistSongItem.ListItem.view.controller.openMv(
                                                ListItemData.mvId,
                                                ListItemData.name,
                                                ListItemData.artistsText)
                                        }
                                        imageSource: "asset:///icons/GridIconVideo.png"
                                    },
                                    ActionItem {
                                        // 音乐详情（Properties）：看这一首的信息 / 歌词
                                        title: qsTr("音乐详情")
                                        onTriggered: {
                                            artistSongItem.ListItem.view.controller.requestOpenPropertiesFor(ListItemData)
                                        }
                                        imageSource: "asset:///icons/ic_info.png"
                                    },
                                    ActionItem {
                                        // 增加到队列：插到当前曲目之后（MusicController::playNextSong）
                                        title: qsTr("增加到队列")
                                        onTriggered: {
                                            var c = artistSongItem.ListItem.view.controller
                                            var ok = c.playNextSong(ListItemData)
                                            c.notifyError(ok
                                                          ? qsTr("已加入队列：%1").arg(ListItemData.name)
                                                          : qsTr("这首歌已经在下一首了"))
                                        }
                                        imageSource: "asset:///icons/ic_add_to_play_next.png"
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
                music.playArtistSong(indexPath[0])
            }
            attachedObjects: [
                ListScrollStateHandler {
                    onScrollingChanged: {
                        music.setScrolling(scrolling)
                        artistPage.headerCollapsed = scrolling || !atBeginning
                    }
                    onAtBeginningChanged: {
                        artistPage.headerCollapsed = scrolling || !atBeginning
                    }
                }
            ]
        }

        // ---- 专辑（独立模型）----
        ListView {
            visible: artistMode.selectedValue == "albums"
            layoutProperties: StackLayoutProperties {
                spaceQuota: 1
            }
            dataModel: music.artistAlbums
            // ★ 同上：委托里拿不到 music，菜单走 controller
            property variant controller: music
            listItemComponents: [
                ListItemComponent {
                    type: ""
                    Container {
                        id: artistAlbumItem
                        contextActions: [
                            ActionSet {
                                title: ListItemData.name
                                subtitle: qsTr("%1 首").arg(ListItemData.trackCount)
                                actions: [
                                    ActionItem {
                                        /*
                                         * 整张专辑追加进队列。
                                         * ★ 用 enqueueAlbum（后端再拉一次曲目后追加），
                                         *   不能用 loadAlbum —— 那个会顶掉当前正在看/播的内容。
                                         */
                                        title: qsTr("增加到队列")
                                        onTriggered: {
                                            artistAlbumItem.ListItem.view.controller.enqueueAlbum(
                                                String(ListItemData.id))
                                        }
                                        imageSource: "asset:///icons/ic_add_to_play_next.png"
                                    }
                                ]
                            }
                        ]
                        PlaylistListItem {
                            title: ListItemData.name
                            subtitle: qsTr("%1 首").arg(ListItemData.trackCount)
                            imageSource: ListItemData.localCoverPath ? ListItemData.localCoverPath : ""
                        }
                    }
                }
            ]
            // ★ 点专辑 → 按【专辑 id】打开专辑页（比按名字搜索准确得多）
            onTriggered: {
                var chosen = dataModel.data(indexPath)
                if (chosen && chosen.id)
                    music.requestOpenAlbum(chosen.id, chosen.name)
            }
            attachedObjects: [
                ListScrollStateHandler {
                    onScrollingChanged: {
                        music.setScrolling(scrolling)
                        artistPage.headerCollapsed = scrolling || !atBeginning
                    }
                    onAtBeginningChanged: {
                        artistPage.headerCollapsed = scrolling || !atBeginning
                    }
                }
            ]
        }

        // ---- 歌曲（已删除：与"热门"重复；只保留热门 / 专辑）----
    }
}
