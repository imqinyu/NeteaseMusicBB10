// UserDetailPage - 个人主页（用户详情）
//
// 结构照 NMDemoUI/assets/UserDetailPage.qml 的想法：
//   顶部一张「背景图 + 头像」（头像是叠在背景上、有前后层次）
//   + 昵称 / 等级 / 累计听歌，下面用 SegmentedControl 切 歌单 / 专辑 / 艺人。
//
// 数据全部复用现有模型（music.playlists / music.albums / music.artists），
// 不新增请求。头像是 C++ 下到本地的路径（music.imagePath）。
//
// ★ 本页是 main.qml 用 ComponentDefinition createObject 推入的独立页面，
//   拿不到 main 的 root，所以列表点击走 music 的「请求 + 版本号」通知
//   （music.requestOpenPlaylist / requestOpenSearch），由 main.qml 的
//   onOpenRequestVersionChanged 负责推页。
//   ★ 千万不要再往页面属性里塞 JS 回调函数：QML 会把它当成绑定产生
//     "Binding loop"，循环抖动还会让 ListView.onTriggered 反复触发，
//     表现为无限推页直到卡死。

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
    id: userDetailPage
    objectName: "userDetailPage"

    // 下载完成后重新求值
    property int artVersion: music.imageCacheVersion
    onArtVersionChanged: refreshBanner()

    // 背景图：没有"用户头图"接口，从缓存里挑一张专辑/歌单封面凑合
    // （★ 不要拿头像放大——半夜看着瘆人）
    function refreshBanner() {
        var p = music.bannerPath()
        bannerImage.imageSource = (p && p.length > 0) ? p : "asset:///images/ic_default.png"
    }
    onCreationCompleted: {
        music.trace("userDetailPage: created, uid=[" + uid + "] isSelf=" + isSelf)
        refreshBanner()
    }

    /*
     * ★★ 「拉别人的歌单」必须挂在 onUidChanged，不能放 onCreationCompleted！
     *
     *   本页是 main.qml 用 createObject 建好、push 之前才注入 uid 的，
     *   而 onCreationCompleted 在 createObject 那一刻就已经跑完了 ——
     *   那时 uid 还是空串（isSelf 为 true），于是永远走不进"看别人"的分支，
     *   歌单列表一直是空的。uid 一赋值这里就会触发，正好赶上。
     */
    onUidChanged: {
        // ★★ 这里绝不能读 isSelf！isSelf 是依赖 uid 的绑定，
        //   在 onUidChanged 触发的这一刻它很可能【还没重算】（读到的还是旧值 true），
        //   于是 if (!isSelf) 恒为假 —— 表现就是"昵称头像后面自己刷对了，
        //   但歌单永远拉不出来"（真机反馈）。直接看 uid 本身最稳。
        music.trace("userDetailPage: uid=[" + uid + "] isSelf=" + isSelf)
        headerExpanded = false
        if (uid.length > 0)
            music.loadViewedPlaylists(uid)
    }

    // 头部三态（照通讯录联系人详情页 ContactHeader）：
    //   headerCollapsed —— 列表滚动中，整块收起
    //   headerExpanded  —— 点一下放大，再点缩回
    property bool headerCollapsed: false
    property bool headerExpanded: false

    /*
     * ★ 这一页现在有两种用法（显示的内容差不多，所以直接复用）：
     *     uid 为空   → 「我」的资料（默认；数据来自 music.playlists/albums/artists）
     *     uid 非空   → 别人的资料（评论区长按「打开用户资料」进来）：
     *                  昵称/头像由评论直接带进 music.viewedUser，
     *                  歌单走 music.viewedPlaylists（和"我的"分开，互不污染）。
     *   别人的「专辑 / 艺人」拿不到（artist/sublist 只对登录用户开放），
     *   所以分段控件对别人隐藏，这一页对别人就是"头像 + 昵称 + 他的歌单"。
     */
    property string uid: ""
    property bool isSelf: uid.length === 0

    property string viewedNickName: (music.viewedUser && music.viewedUser.nickname)
                                    ? music.viewedUser.nickname : ""
    property string viewedAvatarUrl: (music.viewedUser && music.viewedUser.avatarUrl)
                                     ? music.viewedUser.avatarUrl : ""
    /*! 别人的等级 / 听歌数（来自 /api/v1/user/detail/<uid>，见 UserDetailParse） */
    property int viewedLevel: (music.viewedUser && music.viewedUser.level)
                              ? music.viewedUser.level : 0
    property int viewedListenSongs: (music.viewedUser && music.viewedUser.listenSongs)
                                    ? music.viewedUser.listenSongs : 0
    /*! 对方是音乐人时才有（user/detail 的 profile.artistName）→ 「音乐人」入口用 */
    property string viewedArtistName: (music.viewedUser && music.viewedUser.artistName)
                                      ? music.viewedUser.artistName : ""

    /*! 头像是谁的：自己用登录资料，别人用 viewedUser（C++ 那边已下到本地） */
    property string avatarUrl: isSelf ? music.userAvatarUrl : viewedAvatarUrl
    /*! 标题栏 / 头部显示的名字 */
    property string displayName: isSelf
                                 ? (music.nickName.length > 0 ? music.nickName
                                                              : qsTr("未登录"))
                                 : (viewedNickName.length > 0 ? viewedNickName
                                                              : qsTr("用户资料"))

    titleBar: TitleBar {
        title: userDetailPage.displayName
    }

    actions: [
        ActionItem {
            title: qsTr("刷新")
            imageSource: "asset:///icons/ic_reload.png"
            onTriggered: {
                if (userDetailPage.isSelf)
                    music.loadMyProfile()
                else
                    music.loadViewedPlaylists(userDetailPage.uid)
            }
        },
        // 下面两项照 NMDemoUI/assets/UserDetailPage.qml 的骨架补上（接口还没接）
        ActionItem {
            title: qsTr("分享")
            onTriggered: {
                music.notifyError(qsTr("分享还没接后端"))
            }
            imageSource: "asset:///icons/ic_share.png"
        },
        ActionItem {
            title: qsTr("关注")
            onTriggered: {
                music.notifyError(qsTr("关注还没接后端"))
            }
            imageSource: "asset:///icons/ic_add_contact.png"
        }
    ]

    Container {
        horizontalAlignment: HorizontalAlignment.Fill
        verticalAlignment: VerticalAlignment.Fill
        layout: StackLayout {
            orientation: LayoutOrientation.TopToBottom
        }

        // ---- 顶部：背景图 + 头像 + 昵称/等级 ----
        // 照通讯录联系人详情页：默认缩小，点一下放大（约 1:3），再点缩回；
        // 列表一滚动则整块收起（visible=false 不占空间）。
        Container {
            id: profileHeader
            horizontalAlignment: HorizontalAlignment.Fill
            preferredHeight: userDetailPage.headerExpanded ? ui.sdu(56) : ui.sdu(19)
            layout: DockLayout {
            }
            gestureHandlers: [
                TapHandler {
                    onTapped: userDetailPage.headerExpanded = !userDetailPage.headerExpanded
                }
            ]

            // 背景图：单独一张专辑/歌单封面（见 refreshBanner），不再放大头像
            ImageView {
                id: bannerImage
                horizontalAlignment: HorizontalAlignment.Fill
                verticalAlignment: VerticalAlignment.Fill
                scalingMethod: ScalingMethod.AspectFill
                opacity: 0.5
                imageSource: "asset:///images/ic_default.png"
            }

            // 头像 + 文字：贴底部左侧，相对背景有"错位"的层次
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
                    preferredWidth: userDetailPage.headerExpanded ? ui.sdu(30) : ui.sdu(14)
                    preferredHeight: userDetailPage.headerExpanded ? ui.sdu(30) : ui.sdu(14)
                    verticalAlignment: VerticalAlignment.Center
                    scalingMethod: ScalingMethod.AspectFill
                    imageSource: userDetailPage.artVersion >= 0
                                 && userDetailPage.avatarUrl.length > 0
                                 ? music.imagePath(userDetailPage.avatarUrl)
                                 : "asset:///images/avatar_bright.png"
                }

                Container {
                    leftPadding: ui.sdu(2)
                    verticalAlignment: VerticalAlignment.Center
                    layout: StackLayout {
                        orientation: LayoutOrientation.TopToBottom
                    }
                    Label {
                        text: userDetailPage.isSelf
                              ? (music.nickName.length > 0
                                 ? music.nickName + (music.vipType > 0 ? "  VIP" : "")
                                 : qsTr("未登录"))
                              : userDetailPage.viewedNickName
                        textStyle {
                            base: SystemDefaults.TextStyles.TitleText
                        }
                    }
                    Label {
                        // 缩小时只留头像 + 名字，等级这行藏起来。
                        // ★ 自己的用 music.userLevel；别人的用 viewedUser 里那份
                        //   （/api/v1/user/detail 拉的），格式刻意保持一致，
                        //   这样两边的资料页看起来才"差不多"。
                        visible: userDetailPage.headerExpanded
                                 && (userDetailPage.isSelf
                                     || userDetailPage.viewedLevel > 0)
                        text: userDetailPage.isSelf
                              ? (music.userLevel > 0
                                 ? qsTr("Lv.%1 · 累计听歌 %2 首")
                                   .arg(music.userLevel).arg(music.userPlayCount)
                                 : qsTr("在账号管理中新增账号"))
                              : qsTr("Lv.%1 · 累计听歌 %2 首")
                                .arg(userDetailPage.viewedLevel)
                                .arg(userDetailPage.viewedListenSongs)
                        textStyle {
                            base: SystemDefaults.TextStyles.SubtitleText
                            color: ui.palette.textOnPlain
                        }
                    }
                }
            }
        }

        // ---- 分段：歌单 / 艺人（「专辑」那段按需求去掉）----
        // ★ 自己的和别人的都显示这一条：
        //     自己：歌单 = 我的歌单，艺人 = 我关注的艺人
        //     别人：歌单 = 他的歌单，艺人 = 说明 + 「音乐人」入口
        
        SegmentedControl {
            id: detailMode
            // 切分段时把头部放回来（否则会一直停在收起状态）
            onSelectedValueChanged: userDetailPage.headerCollapsed = false
            Option {
                text: qsTr("歌单")
                value: "playlists"
                selected: true
            }
            Option {
                text: qsTr("关注")
                value: "artists"
            }
        }

        // ---- 歌单 ----（看别人时这里就是他的歌单）
        ListView {
            // ★ 两边都得跟着分段切！之前对"别人"写的是恒 true，
            //   结果点「艺人」还是显示歌单（真机反馈）
            visible: detailMode.selectedValue == "playlists"
            layoutProperties: StackLayoutProperties {
                spaceQuota: 1
            }
            dataModel: userDetailPage.isSelf ? music.playlists : music.viewedPlaylists
            property variant controller: music
            listItemComponents: [
                ListItemComponent {
                    type: ""
                    Container {
                        id: udPl
                        contextActions: [
                            ActionSet {
                                title: ListItemData.name
                                subtitle: qsTr("%1 首").arg(ListItemData.trackCount)
                                actions: [
                                    ActionItem {
                                        title: qsTr("加载歌单")
                                        onTriggered: {
                                            udPl.ListItem.view.controller.loadPlaylist(udPl.ListItem.data.id)
                                        }
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
            onTriggered: {
                var chosen = dataModel.data(indexPath)
                if (chosen)
                    music.requestOpenPlaylist(chosen.id)
            }
            attachedObjects: [
                ListScrollStateHandler {
                    onScrollingChanged: {
                        music.setScrolling(scrolling)
                        userDetailPage.headerCollapsed = scrolling || !atBeginning
                    }
                    onAtBeginningChanged: {
                        userDetailPage.headerCollapsed = scrolling || !atBeginning
                    }
                }
            ]
        }

        // ---- 艺人 ----
        ListView {
            // ★ 同专辑：看别人时绝不露出"我的"关注艺人
            visible: userDetailPage.isSelf && detailMode.selectedValue == "artists"
            layoutProperties: StackLayoutProperties {
                spaceQuota: 1
            }
            dataModel: music.artists
            property variant controller: music
            listItemComponents: [
                ListItemComponent {
                    type: ""
                    Container {
                        id: udArtist
                        contextActions: [
                            ActionSet {
                                title: ListItemData.name
                                subtitle: ListItemData.trans
                                actions: [
                                    ActionItem {
                                        title: qsTr("搜索该艺人")
                                        onTriggered: {
                                            udArtist.ListItem.view.controller.search(udArtist.ListItem.data.name)
                                        }
                                    }
                                ]
                            }
                        ]
                        ArtistListItem {
                            title: ListItemData.name
                            subtitle: qsTr("专辑 %1 · MV %2")
                                      .arg(ListItemData.albumSize)
                                      .arg(ListItemData.mvSize)
                            imageSource: ListItemData.localPicPath ? ListItemData.localPicPath : ""
                        }
                    }
                }
            ]
            onTriggered: {
                var chosen = dataModel.data(indexPath)
                if (chosen)
                    music.requestOpenSearch(chosen.name)
            }
            attachedObjects: [
                ListScrollStateHandler {
                    onScrollingChanged: {
                        music.setScrolling(scrolling)
                        userDetailPage.headerCollapsed = scrolling || !atBeginning
                    }
                    onAtBeginningChanged: {
                        userDetailPage.headerCollapsed = scrolling || !atBeginning
                    }
                }
            ]
        }

        // ---- 艺人（别人）：网易没有公开"TA 关注的歌手"这个接口 ----
        // 实测见 tools/probe-user-artists.ps1：artist/sublist 完全不认 uid，
        // 永远返回登录用户自己的关注歌手；换成各种 /v1/... 路径也全是 404。
        // 所以这一栏给一句说明；另外如果对方本人是"音乐人"
        //（/api/v1/user/detail 的 profile.artistId），就提供入口跳他的艺人页。
        Container {
            visible: !userDetailPage.isSelf
                     && detailMode.selectedValue == "artists"
            layoutProperties: StackLayoutProperties {
                spaceQuota: 1
            }
            leftPadding: ui.sdu(2)
            rightPadding: ui.sdu(2)
            topPadding: ui.sdu(2)
            layout: StackLayout {
                orientation: LayoutOrientation.TopToBottom
            }
            Label {
                text: qsTr("接口未返回数据")
                multiline: true
                textStyle {
                    base: SystemDefaults.TextStyles.BodyText
                    color: ui.palette.textOnPlain
                }
            }
            // ★ 用 Container 包着来判断显示（Button 自己的 visible 不保险，
            //   SegmentedControl 就踩过这个坑）
            Container {
                visible: userDetailPage.viewedArtistName.length > 0
                topPadding: ui.sdu(2)
                layout: StackLayout {
                    orientation: LayoutOrientation.TopToBottom
                }
                Label {
                    text: qsTr("不过 TA 本人就是音乐人")
                    textStyle {
                        base: SystemDefaults.TextStyles.BodyText
                    }
                }
                Button {
                    text: qsTr("查看 %1 的艺人页").arg(userDetailPage.viewedArtistName)
                    onClicked: {
                        // 走和别处一样的推页链路（main.qml 的 kind == "artist"）
                        music.requestOpenArtist(userDetailPage.viewedArtistName)
                    }
                }
            }
        }
    }
}
