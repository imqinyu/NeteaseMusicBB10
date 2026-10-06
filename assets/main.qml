// NeteaseMusic - 主界面
//
// 骨架来自 NMDemoUI（用户写的 UI 壳），数据绑定接本项目的后端：
//   context property:  music（逻辑层） / player（全局唯一的 MediaPlayer，C++ 注册）
//
// 三个 Tab（每个都是 NavigationPane，详情页 push 进去，返回用原生返回键/左滑）：
//   资料库：我的歌单 / 艺术家（分段切换）
//   推荐  ：推荐歌单 + 每日推荐歌曲
//   用户  ：头像 / 昵称 / 等级 / 听歌时长
// MV 不再单开 Tab：曲目长按菜单里有「播放 MV」（MvPlayerPage 保留）。
//
// 顶部有 NowPlayingBar（正在播放迷你条，照系统播放器）。
// 设置 / 关于 / 账号管理 / 刷新 在 Menu.definition（BB10 顶部下拉的应用菜单）。
//
// ⚠️ 委托（ListItemComponent）里既看不到外层 id、也看不到 context property：
//    只能用 ListItemData 和 ListItem.<xxx>，且 ListItem.* 要加根节点 id 前缀。

import bb.cascades 1.4
import bb.multimedia 1.0


TabbedPane {
    id: root

    showTabsOnActionBar: false

    // ---- 播放触发 ----
    property string playUrl: music.playUrlVersion ? music.playUrl : ""
    onPlayUrlChanged: {
        if (playUrl.length > 0) {
            player.sourceUrl = playUrl
            player.play()
            // QML 里没法给 context property 的 player 写 onMediaStateChanged，
            // 所以在起播这里申请系统媒体服务
            if (!np.acquired)
                np.acquire()
        }
    }

    property variant nowPlaying: music.currentTrackVersion
                                 ? music.currentTrack : null
    property int artVersion: music.imageCacheVersion

    // 评论拉完（带"要自动打开"标志）→ 推评论页
    property int commentsVersion: music.commentsVersion
    onCommentsVersionChanged: {
        if (music.commentsAutoOpen) {
            music.clearCommentsAutoOpen()
            root.pushPage(commentsPageDef)
        }
    }

    // MV 地址拿到 → 推播放页
    property int mvUrlVersion: music.mvUrlVersion
    onMvUrlVersionChanged: {
        if (music.mvUrl.length > 0)
            root.pushPage(mvPlayerPageDef)
    }

    // 登录成功 → 自动退回（栈顶是登录页时）
    property bool loggedInNow: music.loggedIn
    onLoggedInNowChanged: {
        if (!music.loggedIn)
            return
        var nav = root.activePane
        if (!nav)
            return

        /*
         * ★ 扫码登录页是【叠在登录页之上】推的（栈：… → loginPage →
         *   barcodeLoginPage），所以登录成功后要连着弹两层才回得到进入前的
         *   页面 —— 只弹一层的话会卡在登录页上。
         *
         *   粘贴登录那条路只有一层，第一个 if 不命中，行为不变。
         */
        if (nav.top && nav.top.objectName == "barcodeLoginPage")
            nav.pop()
        if (nav.top && nav.top.objectName == "loginPage")
            nav.pop()
    }

    /*
     * 独立页面（UserDetailPage）请求打开曲目页：它拿不到本页 root，
     * 通过 music 的请求属性 + 版本号通知这里推页
     * （和 commentsAutoOpen 同一套"属性 + 版本号"模式）。
     */
    property int openRequestVersion: music.openRequestVersion
    onOpenRequestVersionChanged: {
        var kind = music.openRequestKind
        var arg = music.openRequestArg
        if (kind.length == 0)
            return
        music.clearOpenRequest()
        if (kind == "playlist")
            root.openPlaylistById(arg)
        else if (kind == "search")
            root.openSearchByName(arg)
        else if (kind == "artist")
            root.pushArtistDetail(arg)
        else if (kind == "album")
            root.openAlbumById(arg)
        else if (kind == "queue")
            root.pushQueue()
        else if (kind == "nowplaying")
            root.pushNowPlaying()
        else if (kind == "properties")
            root.pushProperties()
        else if (kind == "user")
            root.pushUserDetail(arg)
        else if (kind == "login")
            root.pushLogin()
        else if (kind == "barcodeLogin")
            root.pushBarcodeLogin()
    }

    /*
     * 通用推页。
     * ★ createObject 一定要传 parent（传 NavigationPane 本身）：
     *   不传 parent 对象归 JS 所有可能被 GC 回收（推进去又被弹回）；
     *   传 TabbedPane 也不行（Page 的 parent 应该是承载它的 NavigationPane）。
     * ★ 推之前关掉应用菜单：菜单关闭的 peek 动画会被 NavigationPane 当成
     *   返回手势，刚推的页面会被判 isSuggestedStackOk 弹回（官方 TabAppMenu
     *   示例同款处理）。pop 回来时再把菜单打开（见各 NavigationPane）。
     */
    function pushPage(component, extraSetup) {
        music.trace("pushPage: enter")
        var nav = root.activePane
        if (!nav) {
            music.trace("pushPage: NO activePane")
            music.notifyError(qsTr("打开页面失败：无法获取导航栈"))
            return
        }
        var page = component.createObject(nav)
        if (!page) {
            music.trace("pushPage: createObject returned null")
            music.notifyError(qsTr("页面加载失败（QML 文件有错误）"))
            return
        }
        music.trace("pushPage: created " + page.objectName)
        if (extraSetup)
            extraSetup(page)

        /*
         * ★ 推页瞬间先关掉 peek：push 大多由点击/手势触发，同一根手指的
         *   滑动会被 NavigationPane 当成"向左滑返回"，刚推的页立刻被弹回
         *   （真机日志典型表现：push 完马上 isSuggestedStackOk → popped）。
         *   过渡结束（onPushTransitionEnded）再把 peek 打开。
         */
        if (nav.peekEnabled)
            nav.peekEnabled = false

        Application.menuEnabled = false
        nav.push(page)
        music.trace("pushPage: pushed, top=" + (nav.top ? nav.top.objectName : "null"))
    }

    function pushSettings() {
        pushPage(settingsPageDef)
    }
    function pushAbout() {
        pushPage(aboutPageDef)
    }
    function pushSearch() {
        pushPage(searchPageDef)
    }
    function pushArtists() {
        pushPage(artistsPageDef)
    }
    function pushNowPlaying() {
        pushPage(nowPlayingPageDef)
    }
    /* 音乐详情（Properties）：看【当前播放】这首歌的信息 / 歌词 */
    function pushProperties() {
        pushPage(propertiesPageDef)
    }
    /* 播放队列页：正在播放页是独立推出来的页，拿不到 root，
     * 靠 music.requestOpenQueue 发请求转到这里（见 onOpenRequestVersionChanged）*/
    function pushQueue() {
        pushPage(queuePageDef)
    }
    /* 个人主页（UserDetailPage）：独立加载的页面，它要跳曲目页时通过
     * music.requestOpenPlaylist / requestOpenSearch 发请求，
     * 由下面的 onOpenRequestVersionChanged 负责推页。
     *
     * ★ uid 必须传下去！空串 = 看自己（应用菜单里的「个人主页」）；
     *   非空 = 看别人（评论区长按「打开用户资料」）。
     *   漏了这个参数的话，页面里 uid 恒为空，点谁的主页都显示自己的资料。 */
    function pushUserDetail(uid) {
        pushPage(userDetailPageDef, function (p) {
            p.uid = uid ? uid : ""
        })
    }
    /* 艺人资料页：只注入一个字符串 artistName（不是函数，安全） */
    function pushArtistDetail(name) {
        pushPage(artistDetailPageDef, function (p) {
            p.artistName = name
        })
    }
    /*
     * 打开歌单 / 搜索结果 / 专辑页。
     * ★ 这些页面用的是【浏览模型 music.browseSongs】，全程不碰播放队列
     *   music.songs，所以这里不需要再 snapshotList() —— 队列本来就不会被动。
     *   只有用户在页面里点了某首播放（playBrowseSong），队列才会变成那个列表。
     */
    function openPlaylistById(id) {
        music.loadPlaylist(id)
        root.pushPage(playlistPageDef)
    }
    function openSearchByName(name) {
        music.search(name)
        root.pushPage(playlistPageDef)
    }
    function openAlbumById(id) {
        music.loadAlbum(id)
        root.pushPage(playlistPageDef)
    }
    function pushLogin() {
        pushPage(loginPageDef)
    }
    /* 扫码登录页。登录页里点「二维码登录」时用（music.requestOpenBarcodeLogin） */
    function pushBarcodeLogin() {
        pushPage(barcodeLoginPageDef)
    }
    /* 账号管理页（多账号）：新增账号从那一页里点，走 music.requestOpenLogin() */
    function pushAccountManage() {
        pushPage(accountPageDef)
    }
    function pushDebug() {
        pushPage(debugPageDef, function (p) {
            p.mediaStatusText = np.acquired
                ? qsTr("已获取（音量悬浮条应有播放控件）") : qsTr("未获取")
        })
    }

    // 菜单里的「刷新」：刷新当前 Tab 的内容
    function refreshCurrent() {
        switch (root.activeTab) {
        case libraryTab:
            music.loadMyPlaylists()
            break
        case discoverTab:
            music.loadDiscover()
            break
        case mineTab:
            music.loadMyProfile()
            break
        case allSongsTab:
            music.loadAllSongs()
            break
        }
    }

    /*! 当前曲目信息推给系统媒体服务（MetaData 里 title 的键是 "track"） */
    function pushMetaData() {
        if (!nowPlaying)
            return
        var md = { "track": nowPlaying.name,
                   "artist": nowPlaying.artistsText }
        np.setMetaData(md)
        np.trackChange()
    }
    onNowPlayingChanged: pushMetaData()

    // =====================================================================
    // 应用菜单（顶部下拉 / 从左上下滑）
    // =====================================================================
    Menu.definition: MenuDefinition {
        settingsAction: SettingsActionItem {
            onTriggered: {
                root.pushSettings()
            }
        }
        helpAction: HelpActionItem {
            onTriggered: {
                root.pushAbout()
            }
        }
        actions: [
            ActionItem {
                title: qsTr("账号管理")
                imageSource: "asset:///icons/ic_contact.png"
                onTriggered: {
                    // 账号管理页（多账号）；登录页从那里面的「新增账号」进
                    root.pushAccountManage()
                }
            },
            ActionItem {
                title: qsTr("刷新")
                imageSource: "asset:///icons/ic_reload.png"
                onTriggered: {
                    root.refreshCurrent()
                }
            }
        ]
    }

    // ============================ 资料库 ============================
    Tab {
        id: libraryTab
        title: qsTr("资料库")
        imageSource: "asset:///icons/ic_music_library.png"

        NavigationPane {
            id: libraryNav
            objectName: "libraryNav"
            peekEnabled: true
            onPushTransitionEnded: {
                // pushPage 推页时临时关掉了 peek，这里恢复（见 pushPage 注释）
                peekEnabled = true
            }
            onPopTransitionEnded: {
                // 歌单之类的大列表在 pop 前先把数据模型清空，
                // 让 ListView 的 delegate 立即释放，避免返回卡顿几秒。
                // 非歌单页没有 myList 属性，page.myList 为 undefined，跳过。
                // page 可能为 null（返回到根后又多触发一次 pop），必须判空，
                // 否则 page.destroy() 直接抛 TypeError。
                if (page) {
                    // 艺人资料页离开时还原她进来前的列表（见 MusicController::snapshotList）
                    if (page.restoreListOnPop)
                        music.restoreList()
                    if (page.myList)
                        page.myList.dataModel = null
                    page.destroy()
                }
                Application.menuEnabled = true
                peekEnabled = true
            }

            Page {
                id: libraryPage

                actions: [
                    ActionItem {
                        title: qsTr("随机播放")
                        imageSource: "asset:///icons/ic_shuffle_all.png"
                        ActionBar.placement: ActionBarPlacement.OnBar
                        onTriggered: {
                            if (music.songCount() > 0) {
                                var idx = Math.floor(Math.random() * music.songCount())
                                music.playIndex(idx)
                            }
                        }
                    },
                    ActionItem {
                        title: qsTr("最近播放")
                        imageSource: "asset:///icons/ca_recent_contacts.png"
                        ActionBar.placement: ActionBarPlacement.OnBar
                        onTriggered: {
                            // 用本地播放记录填 songs，再进曲目页
                            music.loadRecentPlayed()
                            root.pushPage(playlistPageDef)
                        }
                    },
                    ActionItem {
                        title: qsTr("搜索")
                        imageSource: "asset:///icons/ic_search.png"
                        ActionBar.placement: ActionBarPlacement.Signature
                        onTriggered: {
                            root.pushSearch()
                        }
                    }
                ]

                Container {
                    layout: StackLayout {
                        orientation: LayoutOrientation.TopToBottom
                    }

                    NowPlayingBar {
                        onOpenPlayer: {
                            root.pushNowPlaying()
                        }
                    }

                    SegmentedControl {
                        id: libraryMode
                        Option {
                            text: qsTr("播放列表")
                            value: "playlists"
                            selected: true
                        }
                        Option {
                            text: qsTr("艺术家")
                            value: "artists"
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

                    /*
                     * ★★ 三个列表各用一个 ListView，而不是在一个 ListView 里
                     *   按 type 分派项组件 —— 因为 ArrayDataModel::itemType()
                     *   恒返回空串，自定义 type 根本不生效（NDK 头文件
                     *   arraydatamodel.h 里写明："you need to ... set item=""")。
                     *   所以每个 ListView 只放一个 type:"" 的 ListItemComponent，
                     *   切换分段时用 visible 切换列表。
                     */

                    // ---- 播放列表（按 subscribed 分成「我创建的 / 我收藏的」两组）----
                    ListView {
                        visible: libraryMode.selectedValue == "playlists"
                        layoutProperties: StackLayoutProperties {
                            spaceQuota: 1
                        }
                        layout: StackListLayout {
                            // ★ 同「全部歌曲」：用 Sticky，不要 StickyOverlay
                            //   （后者 header 不占布局空间，会盖住组内第一项）
                            headerMode: ListHeaderMode.Sticky
                        }
                        dataModel: music.libraryPlaylists
                        property variant controller: music
                        listItemComponents: [
                            // ---- 分组 header：我创建的 / 我收藏的 ----
                            // ★★ 分组键是【单字符】"0" / "1"，不是中文字串！
                            //   GroupDataModel 把分组键交给 header 委托时，多字符键
                            //   到 QML 里只剩第一个字（实测 "我创建的" → "我"）。
                            //   「全部歌曲」「艺术家」的键本来就都是单字符，所以从没
                            //   暴露过这个问题。显示文案在这里查表。
                            ListItemComponent {
                                type: "header"
                                Header {
                                    title: ListItemData === "0" ? qsTr("我创建的")
                                         : ListItemData === "1" ? qsTr("我收藏的")
                                         : ListItemData
                                }
                            },
                            ListItemComponent {
                                type: "item"
                                Container {
                                    id: plItem
                                    contextActions: [
                                        ActionSet {
                                            title: ListItemData.name
                                            subtitle: qsTr("%1 首").arg(ListItemData.trackCount)
                                            actions: [
                                                ActionItem {
                                                    title: qsTr("加载歌单")
                                                    onTriggered: {
                                                        plItem.ListItem.view.controller.loadPlaylist(plItem.ListItem.data.id)
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
                            if (chosen) {
                                music.loadPlaylist(chosen.id)
                                root.pushPage(playlistPageDef)
                            }
                        }
                        attachedObjects: [
                            ListScrollStateHandler {
                                onScrollingChanged: {
                                    music.setScrolling(scrolling)
                                }
                            }
                        ]
                    }

                    // （原「专辑」分区已按需求移除，资料库只保留播放列表与艺术家）

                    // ---- 艺术家（按艺人归类「全部歌曲」，像 Apple Music）----
                    // 空状态：手上还没有曲目数据时（没进过「全部歌曲」页、也没有缓存）
                    Container {
                        visible: libraryMode.selectedValue == "artists"
                                 && music.libraryArtistCount == 0
                        leftPadding: ui.sdu(2)
                        rightPadding: ui.sdu(2)
                        topPadding: ui.sdu(2)
                        Label {
                            text: qsTr("还没有曲目数据。先去「全部歌曲」页加载一次，"
                                       + "这里就会按艺人归好类。")
                            multiline: true
                            textStyle {
                                base: SystemDefaults.TextStyles.BodyText
                                color: ui.palette.textOnPlain
                            }
                        }
                    }

                    // ★ 数据是本地这份列表算出来的（music.libraryArtists，见
                    //   MusicController::rebuildLibraryArtists），不联网、也不需要
                    //   "关注的艺人"接口。
                    ListView {
                        visible: libraryMode.selectedValue == "artists"
                        layoutProperties: StackLayoutProperties {
                            spaceQuota: 1
                        }
                        layout: StackListLayout {
                            // ★ 同「全部歌曲」：用 Sticky，不要 StickyOverlay
                            //   （后者 header 不占布局空间，会盖住组内第一项）
                            headerMode: ListHeaderMode.Sticky
                        }
                        dataModel: music.libraryArtists
                        property variant controller: music
                        listItemComponents: [
                            // ---- 分组 header（首字符）----
                            // 数字/符号那组在 C++ 里是 U+FFFF 哨兵（保证排最后），显示时改回 "*"
                            ListItemComponent {
                                type: "header"
                                Header {
                                    title: ListItemData === "\uFFFF" ? "*" : ListItemData
                                }
                            },
                            ListItemComponent {
                                type: "item"
                                Container {
                                    id: libArtist
                                    contextActions: [
                                        ActionSet {
                                            title: ListItemData.name
                                            subtitle: qsTr("%1 首").arg(ListItemData.songCount)
                                            actions: [
                                                ActionItem {
                                                    title: qsTr("搜索艺人")
                                                    onTriggered: {
                                                        libArtist.ListItem.view.controller.search(libArtist.ListItem.data.name)
                                                    }
                                                    imageSource: "asset:///icons/ic_search.png"
                                                },
                                                ActionItem {
                                                    title: qsTr("打开资料")
                                                    onTriggered: {
                                                        libArtist.ListItem.view.controller.requestOpenArtist(libArtist.ListItem.data.name)
                                                    }
                                                    imageSource: "asset:///icons/ic_open_contacts.png"
                                                }
                                            ]
                                        }
                                    ]
                                    ArtistListItem {
                                        title: ListItemData.name
                                        subtitle: qsTr("%1 首").arg(ListItemData.songCount)
                                        imageSource: ListItemData.localCoverPath ? ListItemData.localCoverPath : ""
                                    }
                                }
                            }
                        ]
                        onTriggered: {
                            var chosen = dataModel.data(indexPath)
                            if (chosen) {
                                // 把他名下的歌做成一个列表，然后复用播放列表页
                                music.loadLibraryArtist(chosen.name)
                                root.pushPage(playlistPageDef)
                            }
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
        }
    }

    // ============================ 推荐 ============================
    Tab {
        id: discoverTab
        title: qsTr("推荐")
        imageSource: "asset:///icons/ic_connections.png"

        NavigationPane {
            id: discoverNav
            objectName: "discoverNav"
            peekEnabled: true
            onPushTransitionEnded: {
                // pushPage 推页时临时关掉了 peek，这里恢复（见 pushPage 注释）
                peekEnabled = true
            }
            onPopTransitionEnded: {
                // 歌单之类的大列表在 pop 前先把数据模型清空，
                // 让 ListView 的 delegate 立即释放，避免返回卡顿几秒。
                // 非歌单页没有 myList 属性，page.myList 为 undefined，跳过。
                // page 可能为 null（返回到根后又多触发一次 pop），必须判空，
                // 否则 page.destroy() 直接抛 TypeError。
                if (page) {
                    // 艺人资料页离开时还原她进来前的列表（见 MusicController::snapshotList）
                    if (page.restoreListOnPop)
                        music.restoreList()
                    if (page.myList)
                        page.myList.dataModel = null
                    page.destroy()
                }
                Application.menuEnabled = true
                peekEnabled = true
            }

            Page {
                id: discoverPage

                actionBarAutoHideBehavior: ActionBarAutoHideBehavior.HideOnScroll

                actionBarVisibility: ChromeVisibility.Compact
                Container {
                    layout: StackLayout {
                        orientation: LayoutOrientation.TopToBottom
                    }

                    NowPlayingBar {
                        onOpenPlayer: {
                            root.pushNowPlaying()
                        }
                    }

                    SegmentedControl {
                        id: discoverMode
                        /*
                         * 切到「歌曲」且还没拉过每日推荐 → 自动拉一次。
                         * ★ 推荐歌曲现在是独立模型（recommendSongs），不再跟随
                         *   全局 songs；不主动拉的话这一页就是空的。
                         */
                        onSelectedValueChanged: {
                            /*
                             * ★ 去掉 `! music.loading` 这个条件！
                             *   loading 是【全局】的：启动时一堆请求在跑，切到
                             *   「歌曲」那一刻它多半是 true，这次加载就被跳过；
                             *   而 onSelectedValueChanged 不会再触发一次，于是
                             *   推荐一直空着、页面永远停在「登录以加载推荐」
                             *   （真机反馈）。
                             *   切换动作本身只触发一次，不会造成重复请求。
                             */
                            if (selectedValue == "songs"
                                    && music.recommendSongsVersion == 0)
                                music.loadRecommendSongs()
                        }
                        Option {
                            text: qsTr("歌单")
                            value: "playlists"
                            selected: true
                        }
                        Option {
                            text: qsTr("歌曲")
                            value: "songs"
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

                    // ---- 推荐歌单 ----
                    ListView {
                        visible: discoverMode.selectedValue == "playlists"
                        layoutProperties: StackLayoutProperties {
                            spaceQuota: 1
                        }
                        dataModel: music.discoverPlaylists
                        property variant controller: music
                        listItemComponents: [
                            ListItemComponent {
                                type: ""
                                Container {
                                    id: dpl
//                                    contextActions: [
//                                        ActionSet {
//                                            title: ListItemData.name
//                                            subtitle: ListItemData.name
//                                            actions: [
//                                                ActionItem {
//                                                    title: qsTr("打开")
//                                                    onTriggered: {
//                                                        dpl.ListItem.view.controller.loadPlaylist(dpl.ListItem.data.id)
//                                                    }
//                                                    imageSource: "asset:///icons/ic_open.png"
//                                                }
//                                            ]
//                                        }
//                                    ]
                                    PlaylistListItem {
                                        title: ListItemData.name
                                        subtitle: ListItemData.description
                                                  ? ListItemData.description
                                                  : qsTr("%1 首").arg(ListItemData.trackCount)
                                        imageSource: ListItemData.localCoverPath ? ListItemData.localCoverPath : ""
                                    }
                                }
                            }
                        ]
                        onTriggered: {
                            var chosen = dataModel.data(indexPath)
                            if (chosen) {
                                music.loadPlaylist(chosen.id)
                                root.pushPage(playlistPageDef)
                            }
                        }
                        attachedObjects: [
                            ListScrollStateHandler {
                                onScrollingChanged: {
                                    music.setScrolling(scrolling)
                                }
                            }
                        ]
                    }

                    // 推荐歌曲为空时的提示（独立模型，未拉过就是空的）
                    Container {
                        visible: discoverMode.selectedValue == "songs"
                                 && music.recommendSongsVersion == 0 && ! music.loading
                        leftPadding: ui.sdu(2)
                        rightPadding: ui.sdu(2)
                        topPadding: ui.sdu(3)
                        Label {
                            text: qsTr("登录以加载推荐")
                            textStyle {
                                base: SystemDefaults.TextStyles.BodyText
                                color: ui.palette.textOnPlain
                            }
                        }
                    }

                    // ---- 歌曲（每日推荐，独立模型：不会被搜索/歌单"污染"）----
                    ListView {
                        id: discoverSongsList
                        visible: discoverMode.selectedValue == "songs"
                        layoutProperties: StackLayoutProperties {
                            spaceQuota: 1
                        }
                        dataModel: music.recommendSongs
                        property variant controller: music
                        listItemComponents: [
                            ListItemComponent {
                                type: ""
                                Container {
                                    id: dsong
                                    contextActions: [
                                        ActionSet {
                                            title: ListItemData.name
                                            subtitle: ListItemData.artistsText
                                            actions: [
                                                ActionItem {
                                                    title: qsTr("播放")
                                                    onTriggered: {
                                                        dsong.ListItem.view.controller.playRecommendSong(dsong.ListItem.indexPath[0])
                                                    }
                                                    imageSource: "asset:///icons/ic_play.png"
                                                },
                                                ActionItem {
                                                    title: qsTr("播放 MV")
                                                    enabled: ListItemData.hasMv === true
                                                    onTriggered: {
                                                        dsong.ListItem.view.controller.openMv(
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
                                                        dsong.ListItem.view.controller.requestOpenPropertiesFor(ListItemData)
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
                            music.playRecommendSong(indexPath[0])
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
        }
    }

    // （原「MV」Tab 已移除：MV 改为在曲目长按菜单里「播放 MV」）

    // ============================ 全部歌曲 ============================
    // 照黑莓官方音乐「All Songs」：整合展示曲目 + 按首字符分组 header，
    // 正在播放的曲目文字加粗高亮。数据 =「我喜欢的音乐」歌单（缓存优先）。
    Tab {
        id: allSongsTab
        title: qsTr("全部歌曲")
        imageSource: "asset:///icons/ic_music.png"
        /*
         * ★ 只在【第一次】进这个 tab 时加载一次。
         *   之后只有两种情况下才刷新：
         *     1) 用户手动点应用菜单里的「刷新」（见 refreshCurrent）
         *     2) 设置里改了「隐藏 VIP 歌曲」开关（C++ 那边会 rebuildAllSongs）
         *   不再每次切回来都重拉一遍。
         */
        property bool loadedOnce: false
        onTriggered: {
            if (!loadedOnce) {
                loadedOnce = true
                music.loadAllSongs()
            }
        }

        NavigationPane {
            id: allSongsNav
            objectName: "allSongsNav"
            peekEnabled: true
            onPushTransitionEnded: {
                peekEnabled = true
            }
            onPopTransitionEnded: {
                if (page) {
                    if (page.restoreListOnPop)
                        music.restoreList()
                    if (page.myList)
                        page.myList.dataModel = null
                    page.destroy()
                }
                Application.menuEnabled = true
                peekEnabled = true
            }

            Page {
                actions: [
                    ActionItem {
                        /*
                         * ★ 随机播放只在【全部歌曲】这份列表里随机：
                         *   后端把这份列表洗牌后当播放队列起播（playAllSongsShuffled），
                         *   不能用 music.songCount()——那是全局 songs，可能是别的列表。
                         */
                        title: qsTr("随机播放")
                        imageSource: "asset:///icons/ic_shuffle_all.png"
                        ActionBar.placement: ActionBarPlacement.OnBar
                        onTriggered: {
                            music.playAllSongsShuffled()
                        }
                    },
                
                    ActionItem {
                        /*
                         * ★ 页内搜索：只过滤本页这批曲目（歌名/艺人/专辑），
                         *   不再是全局搜索页。
                         */
                        title: qsTr("搜索")
                        imageSource: "asset:///icons/ic_search.png"
                        ActionBar.placement: ActionBarPlacement.Signature
                        onTriggered: {
                            allSongsPage.searchVisible = !allSongsPage.searchVisible
                            if (!allSongsPage.searchVisible) {
                                allSongsSearch.text = ""
                                music.setAllSongsFilter("")
                            }
                        }
                    }
                ]
                id: allSongsPage

                /*! 搜索框是否展开（由上面「搜索」动作切换） */
                property bool searchVisible: false

                Container {
                    layout: StackLayout {
                        orientation: LayoutOrientation.TopToBottom
                    }

                    NowPlayingBar {
                        onOpenPlayer: {
                            root.pushNowPlaying()
                        }
                    }

                    // ---- 页内搜索框：只过滤「全部歌曲」这份列表 ----
                    TextField {
                        id: allSongsSearch
                        visible: allSongsPage.searchVisible
                        hintText: qsTr("搜索歌名 / 艺人 / 专辑")
                        leftMargin: ui.sdu(2)
                        rightMargin: ui.sdu(2)
                        bottomMargin: ui.sdu(1)
                        input.submitKey: SubmitKey.Search
                        onTextChanging: {
                            music.setAllSongsFilter(text)
                        }
                    }

                    // 加载中
                    Container {
                        visible: music.loading
                        leftPadding: ui.sdu(2)
                        rightPadding: ui.sdu(2)
                        layout: StackLayout {
                            orientation: LayoutOrientation.LeftToRight
                        }
                     
                    }

                    ListView {
                        objectName: "allSongsListView"
                        layoutProperties: StackLayoutProperties {
                            spaceQuota: 1
                        }
                        // ★ 分组 header 置顶常驻：GroupDataModel 是树状结构
                        //   （root → 分组 → 组内曲目），StickyOverlay 会让分组条停在
                        //   可视区顶部，直到被下一个分组顶掉 —— 官方 All Songs 同款。
                        layout: StackListLayout {
                            // ★ 用 Sticky，不要 StickyOverlay：
                            //   StickyOverlay 的 header 【不占布局空间】，会直接盖在
                            //   组内第一首上、并把整组上移（表现为错位）。
                            headerMode: ListHeaderMode.Sticky
                        }
                        dataModel: music.allSongs
                        property variant controller: music

                        listItemComponents: [
                            // ---- 分组 header（首字符）：GroupDataModel 的分组头 ----
                            // 分组头的数据就是分组名本身（不是 map）。
                            // ★ 数字/符号那一组在 C++ 里用哨兵键 "\uFFFF"（保证排到最后），
                            //   显示时再改回 "*"。
                            ListItemComponent {
                                type: "header"
                                Header {
                                    title: ListItemData === "\uFFFF" ? "*" : ListItemData
                                }
                            },
                            // ---- 曲目行 ----
                            // ★ GroupDataModel 的 itemType()：标题项 = "header"，
                            //   其余所有项 = "item"（官方教材《第6章 ListView 和 DataModel》
                            //   原文："为标题项返回 header，为所有其他项返回 item"）。
                            //   写成别的值（"" / "song"）就匹配不上，ListView 会退化成
                            //   拿 header 组件渲染每一行 —— 表现就是每行只显示一个歌曲 id。
                            ListItemComponent {
                                type: "item"
                                Container {
                                    id: asRow

                                    // ---- 歌曲 ----
                                    Container {
                                        id: asSong
                                        visible: ListItemData.isHeader !== true
                                        contextActions: [
                                            ActionSet {
                                                title: ListItemData.name
                                                subtitle: ListItemData.artistsText
                                                actions: [
                                                    ActionItem {
                                                        title: qsTr("播放")
                                                        onTriggered: {
                                                            asRow.ListItem.view.controller.playAllSongById(String(ListItemData.id))
                                                        }
                                                        imageSource: "asset:///icons/ic_play.png"
                                                    },
                                                    ActionItem {
                                                        // 只把这一首排到下一首（不整列表替换，所以不走 playAllSongById）
                                                        title: qsTr("下一首播放")
                                                        onTriggered: {
                                                            var c = asRow.ListItem.view.controller
                                                            var ok = c.playNextSong(ListItemData)
                                                            c.notifyError(ok
                                                                          ? qsTr("已加入下一首：%1").arg(ListItemData.name)
                                                                          : qsTr("这首歌已经在下一首了"))
                                                        }
                                                        imageSource: "asset:///icons/ic_add_to_play_next.png"
                                                    },
                                                    ActionItem {
                                                        title: qsTr("播放 MV")
                                                        enabled: ListItemData.hasMv === true
                                                        onTriggered: {
                                                            asRow.ListItem.view.controller.openMv(
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
                                                            asRow.ListItem.view.controller.requestOpenPropertiesFor(ListItemData)
                                                        }
                                                        imageSource: "asset:///icons/ic_info.png"
                                                    },
                                                    ActionItem {
                                                        title: qsTr("查看评论")
                                                        onTriggered: {
                                                            // 模型是两层 indexPath，必须按 id 走
                                                            asRow.ListItem.view.controller.requestCommentsById(String(ListItemData.id))
                                                        }
                                                        imageSource: "asset:///icons/ic_chat_multiperson.png"
                                                    }
                                                ]
                                            }
                                        ]
                                        SongListItem {
                                            title: ListItemData.name
                                            subtitle: ListItemData.artistsText
                                            // 官方「全部歌曲」不显示封面：不加载、不占位，省时间
                                            showImage: false
                                            imageSource: ListItemData.localArtPath ? ListItemData.localArtPath : ""
                                            durationText: ListItemData.durationText
                                            isVip: ListItemData.isVip
                                            hasMv: ListItemData.hasMv === true
                                            // ★ 委托里看不到 context property `music`（见文件头 ⚠️），
                                            //   必须走 ListItem.view.controller（ListView 上的
                                            //   `property variant controller: music`）
                                            playing: ListItemData.isHeader !== true
                                                     && asRow.ListItem.view.controller.currentTrackVersion > 0
                                                     && String(ListItemData.id)
                                                        === String(asRow.ListItem.view.controller.currentTrack.id)
                                        }
                                    }
                                }
                            }
                        ]

                        onTriggered: {
                            // GroupDataModel 的 indexPath 是 [组, 项] 两层，按 id 播
                            var item = dataModel.data(indexPath)
                            if (item && item.id !== undefined)
                                music.playAllSongById(String(item.id))
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
        }
    }

    // ============================ 用户 ============================
    Tab {
        id: mineTab
        title: qsTr("用户")
        imageSource: "asset:///icons/ic_myworld.png"

        NavigationPane {
            id: mineNav
            objectName: "mineNav"
            peekEnabled: true
            onPushTransitionEnded: {
                // pushPage 推页时临时关掉了 peek，这里恢复（见 pushPage 注释）
                peekEnabled = true
            }
            onPopTransitionEnded: {
                // 歌单之类的大列表在 pop 前先把数据模型清空，
                // 让 ListView 的 delegate 立即释放，避免返回卡顿几秒。
                // 非歌单页没有 myList 属性，page.myList 为 undefined，跳过。
                // page 可能为 null（返回到根后又多触发一次 pop），必须判空，
                // 否则 page.destroy() 直接抛 TypeError。
                if (page) {
                    // 艺人资料页离开时还原她进来前的列表（见 MusicController::snapshotList）
                    if (page.restoreListOnPop)
                        music.restoreList()
                    if (page.myList)
                        page.myList.dataModel = null
                    page.destroy()
                }
                Application.menuEnabled = true
                peekEnabled = true
            }

            Page {
                id: minePage

                actionBarVisibility: ChromeVisibility.Compact
                /*
                 * 头部三态（照通讯录联系人详情页 ContactHeader 的
                 * minimized / maximized 做法）：
                 *   headerCollapsed —— 列表滚动中，整块收起（不占空间）
                 *   headerExpanded  —— 用户点一下放大，再点缩回
                 */
                property bool headerCollapsed: false
                property bool headerExpanded: false

               

                Container {
                    layout: StackLayout {
                        orientation: LayoutOrientation.TopToBottom
                    }

                    NowPlayingBar {
                        onOpenPlayer: {
                            root.pushNowPlaying()
                        }
                    }

                    // ---- 顶部：背景图 + 头像 + 昵称/等级（照 UserDetailPage）----
                    // 照通讯录联系人详情页 ContactHeader：**默认是缩小的**，
                    // 点一下才放大（DefaultImageHeight:MaximizedImageHeight ≈ 1:3），
                    // 再点缩回；列表一滚动则整块收起。
                    Container {
                        id: mineHeader
                        horizontalAlignment: HorizontalAlignment.Fill
                        preferredHeight: minePage.headerExpanded ? ui.sdu(56) : ui.sdu(19)
                        layout: DockLayout {
                        }
                        gestureHandlers: [
                            TapHandler {
                                onTapped: minePage.headerExpanded = !minePage.headerExpanded
                            }
                        ]

                        ImageView {
                            id: mineBannerImage
                            horizontalAlignment: HorizontalAlignment.Fill
                            verticalAlignment: VerticalAlignment.Fill
                            scalingMethod: ScalingMethod.AspectFill
                            opacity: 0.5
                            // root.artVersion 参与表达式：封面下载完会重新求值
                            imageSource: (root.artVersion >= 0 && music.bannerPath().length > 0)
                                         ? music.bannerPath()
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
                                preferredWidth: minePage.headerExpanded ? ui.sdu(30) : ui.sdu(14)
                                preferredHeight: minePage.headerExpanded ? ui.sdu(30) : ui.sdu(14)
                                verticalAlignment: VerticalAlignment.Center
                                scalingMethod: ScalingMethod.AspectFill
                                imageSource: artVersion >= 0 && music.userAvatarUrl.length > 0
                                             ? music.imagePath(music.userAvatarUrl)
                                             : "asset:///images/avatar_bright.png"
                            }

                            Container {
                                leftPadding: ui.sdu(2)
                                verticalAlignment: VerticalAlignment.Center
                                layout: StackLayout {
                                    orientation: LayoutOrientation.TopToBottom
                                }
                                Label {
                                    text: music.nickName.length > 0
                                          ? music.nickName + (music.vipType > 0 ? "  VIP" : "")
                                          : qsTr("未登录")
                                    textStyle {
                                        base: SystemDefaults.TextStyles.TitleText
                                    }
                                }
                                Label {
                                    // 缩小时只留头像 + 名字，等级这行藏起来
                                    visible: minePage.headerExpanded
                                    text: music.userLevel > 0
                                          ? qsTr("Lv.%1 · 累计听歌 %2 首")
                                            .arg(music.userLevel).arg(music.userPlayCount)
                                          : qsTr("登录后可在应用菜单里登录")
                                    textStyle {
                                        base: SystemDefaults.TextStyles.SubtitleText
                                        color: ui.palette.textOnPlain
                                    }
                                }
                            }
                        }
                    }

                    // ---- 分段：歌单 / 艺人（「专辑」那段按需求去掉）----
                    SegmentedControl {
                        id: mineMode
                        // 切分段时把头部放回来（否则会一直停在收起状态）
                        onSelectedValueChanged: minePage.headerCollapsed = false
                        Option {
                            text: qsTr("歌单")
                            value: "playlists"
                            selected: true
                        }
                        Option {
                            text: qsTr("艺人")
                            value: "artists"
                        }
                    }

                    // ---- 歌单 ----
                    ListView {
                        visible: mineMode.selectedValue == "playlists"
                        layoutProperties: StackLayoutProperties {
                            spaceQuota: 1
                        }
                        dataModel: music.playlists
                        property variant controller: music
                        listItemComponents: [
                            ListItemComponent {
                                type: ""
                                PlaylistListItem {
                                    title: ListItemData.name
                                    subtitle: qsTr("%1 首").arg(ListItemData.trackCount)
                                    imageSource: ListItemData.localCoverPath ? ListItemData.localCoverPath : ""
                                }
                            }
                        ]
                        onTriggered: {
                            var chosen = dataModel.data(indexPath)
                            if (chosen) {
                                music.loadPlaylist(chosen.id)
                                root.pushPage(playlistPageDef)
                            }
                        }
                        attachedObjects: [
                            ListScrollStateHandler {
                                onScrollingChanged: {
                                    music.setScrolling(scrolling)
                                    minePage.headerCollapsed = scrolling || !atBeginning
                                }
                                onAtBeginningChanged: {
                                    minePage.headerCollapsed = scrolling || !atBeginning
                                }
                            }
                        ]
                    }

                    // ---- 艺人 ----
                    ListView {
                        visible: mineMode.selectedValue == "artists"
                        layoutProperties: StackLayoutProperties {
                            spaceQuota: 1
                        }
                        dataModel: music.artists
                        property variant controller: music
                        listItemComponents: [
                            ListItemComponent {
                                type: ""
                                ArtistListItem {
                                    title: ListItemData.name
                                    subtitle: qsTr("专辑 %1 · MV %2")
                                              .arg(ListItemData.albumSize)
                                              .arg(ListItemData.mvSize)
                                    imageSource: ListItemData.localPicPath ? ListItemData.localPicPath : ""
                                }
                            }
                        ]
                        onTriggered: {
                            var chosen = dataModel.data(indexPath)
                            if (chosen)
                                root.pushArtistDetail(chosen.name)
                        }
                        attachedObjects: [
                            ListScrollStateHandler {
                                onScrollingChanged: {
                                    music.setScrolling(scrolling)
                                    minePage.headerCollapsed = scrolling || !atBeginning
                                }
                                onAtBeginningChanged: {
                                    minePage.headerCollapsed = scrolling || !atBeginning
                                }
                            }
                        ]
                    }
                }
            }
        }
    }

    attachedObjects: [
        /*
         * 播放器【不在】这里：由 applicationui.cpp 建好并注册成
         * context property("player")，全局唯一（详见那里的注释）。
         */

        // ---- 系统媒体服务（音量悬浮条 / 锁屏 / 耳机线控） ----
        NowPlayingConnection {
            id: np
            connectionName: "neteaseMusic"
            // Fancy 才会显示播放控制键（官方 nowplaying 示例同款）
            overlayStyle: OverlayStyle.Fancy
            nextEnabled: true
            previousEnabled: true

            // 用绑定同步（别在 JS 里给这三个赋值，会打断绑定）
            duration: player.duration
            position: player.position
            mediaState: player.mediaState

            iconUrl: nowPlaying && artVersion >= 0
                     && music.imagePath(nowPlaying.artUrl).length > 0
                     ? music.imagePath(nowPlaying.artUrl)
                     : "asset:///images/ic_default.png"
            onAcquired: {
                pushMetaData()
            }
            onPlay: {
                if (playUrl.length > 0)
                    player.play()
            }
            onPause: {
                player.pause()
            }
            onNext: {
                music.next()
            }
            onPrevious: {
                music.prev()
            }
            onRevoked: {
                player.stop()
            }
        },

        // ---- 可推入 NavigationPane 的子页面 ----
        ComponentDefinition {
            id: commentsPageDef
            source: "CommentsPage.qml"
        },
        ComponentDefinition {
            id: mvPlayerPageDef
            source: "MvPlayerPage.qml"
        },
        ComponentDefinition {
            id: settingsPageDef
            source: "SettingsPage.qml"
        },
        ComponentDefinition {
            id: debugPageDef
            source: "DebugPage.qml"
        },
        ComponentDefinition {
            id: loginPageDef
            source: "LoginPage.qml"
        },
        ComponentDefinition {
            id: barcodeLoginPageDef
            source: "BarcodeLoginPage.qml"
        },
        ComponentDefinition {
            id: searchPageDef
            source: "SearchPage.qml"
        },
        ComponentDefinition {
            id: artistsPageDef
            source: "ArtistsPage.qml"
        },
        ComponentDefinition {
            id: nowPlayingPageDef
            source: "NowPlayingPage.qml"
        },
        ComponentDefinition {
            id: propertiesPageDef
            source: "PropertiesPage.qml"
        },
        ComponentDefinition {
            id: userDetailPageDef
            source: "UserDetailPage.qml"
        },
        ComponentDefinition {
            id: artistDetailPageDef
            source: "ArtistDetailPage.qml"
        },
        ComponentDefinition {
            id: aboutPageDef
            source: "AboutPage.qml"
        },
        ComponentDefinition {
            id: playlistPageDef
            source: "PlaylistPage.qml"
        },
        ComponentDefinition {
            id: queuePageDef
            source: "QueuePage.qml"
        },
        ComponentDefinition {
            id: accountPageDef
            source: "AccountManagePage.qml"
        }
    ]

    // 启动只拉推荐歌单（MV 列表进 Tab 时再拉，避免开局抢带宽）
    onCreationCompleted: {
        music.loadDiscover()
    }
}
