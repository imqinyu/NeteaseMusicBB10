#include "MusicController.hpp"

#include <bb/cascades/AbstractPane>
#include <bb/cascades/Application>
#include <bb/cascades/ArrayDataModel>
#include <bb/cascades/GroupDataModel>
#include <bb/cascades/ThemeSupport>
#include <bb/system/Clipboard>
#include <bb/system/SystemToast>
#include <bb/system/SystemUiPosition>
#include <bb/multimedia/MediaPlayer>

#include <QHash>
#include <QLatin1String>
#include <QMap>
#include <QPair>
#include <QtAlgorithms>
#include <QSettings>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include "util/NmAudioCache.hpp"
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>

#include "net/NmCookieStore.hpp"
#include "session/NmSession.hpp"
#include "util/NmImageCache.hpp"
#include "util/NmTr.hpp"

using namespace bb::cascades;

MusicController::MusicController(QObject *parent)
    : QObject(parent)
    , m_http(new nm::NmHttpClient(this))
    , m_session(new nm::NmSession(this))
    , m_api(new nm::NmApi(this))
    , m_imageCache(new nm::NmImageCache(this))
    , m_bigImageCache(new nm::NmImageCache(this))
    , m_mediaPlayer(0)
    , m_songs(new ArrayDataModel(this))
    , m_browseSongs(new ArrayDataModel(this))
    , m_playlists(new ArrayDataModel(this))
    , m_playlistsVersion(0)
    // 只按 sectionKey 分组（见 m_allSongs 的说明）
    , m_libraryPlaylists(new GroupDataModel(QStringList() << QLatin1String("sectionKey"), this))
    , m_libraryPlaylistsVersion(0)
    , m_viewedPlaylists(new ArrayDataModel(this))
    , m_viewedUserMode(false)
    , m_otherAccounts(new ArrayDataModel(this))
    , m_accountsVersion(0)
    // 只按 sectionKey 分组（多给键会让"每个条目各自成组"，见 m_allSongs 的说明）
    , m_libraryArtists(new GroupDataModel(QStringList() << QLatin1String("sectionKey"), this))
    , m_libraryArtistsVersion(0)
    , m_discoverPlaylists(new ArrayDataModel(this))
    , m_discoverVersion(0)
    , m_comments(new ArrayDataModel(this))
    , m_commentsVersion(0)
    , m_commentTotal(0)
    , m_commentsAutoOpen(false)
    , m_artists(new ArrayDataModel(this))
    , m_artistsVersion(0)
    , m_albums(new ArrayDataModel(this))
    , m_albumsVersion(0)
    , m_mvs(new ArrayDataModel(this))
    , m_mvsVersion(0)
    /*
     * ★ 只按 sectionKey 一个键分组。
     *   给多个 sortingKeys 会让"每个条目各自成组"（每个键都参与分组），
     *   分组头的数据就变成了一串键值/下标而不是组名。
     *   组内顺序 = 插入顺序，而 rebuildAllSongs() 已经按 name 排好，
     *   所以组内依然是有序的。
     */
    , m_allSongs(new GroupDataModel(QStringList() << QLatin1String("sectionKey"), this))
    , m_allSongsVersion(0)
    , m_allSongsMode(false)
    , m_allSongsFetching(false)
    , m_mvUrlVersion(0)
    , m_currentMvId(0)
    , m_propertiesLyricFetching(false)
    , m_propertiesVersion(0)
    , m_coverEvalCount(0)
    , m_coverReady(false)
    , m_coverDiagPath()
    , m_coverNudge(0)
    , m_userLevel(0)
    , m_userPlayCount(0)
    , m_vipType(0)
    , m_userId(0)
    , m_pendingPlaylistsLoad(false)
    , m_imageCacheVersion(0)
    , m_imageRefreshPending(false)
    , m_scrolling(false)
    , m_imageRefreshTimer(new QTimer(this))
    , m_playlistInfoVersion(0)
    , m_currentTrackVersion(0)
    , m_currentIndex(-1)
    , m_playUrlVersion(0)
    , m_pendingSongUrlId(0)
    , m_loading(false)
    , m_loggedIn(false)
    , m_openRequestVersion(0)
    , m_hasSnapshot(false)
    , m_playlistsFetching(false)
    , m_profileFetching(false)
    , m_artistSongs(new ArrayDataModel(this))
    , m_artistAlbums(new ArrayDataModel(this))
    , m_artistSongsVersion(0)
    , m_artistAlbumsVersion(0)
    , m_artistMode(false)
    , m_recommendSongs(new ArrayDataModel(this))
    , m_recommendSongsVersion(0)
    , m_recommendMode(false)
    , m_searchSongs(new ArrayDataModel(this))
    , m_searchSongsVersion(0)
    , m_repeatMode(1)
    , m_shuffle(false)
    , m_enqueueAlbumMode(false)
    , m_queueAdopted(false)
    , m_browseMode(false)
    , m_browseSongsVersion(0)
    , m_lyricsEnabled(false)
    , m_lyricsTransEnabled(false)
    , m_playerPosition(0)
    , m_seeking(false)
    , m_hideVip(false)
    , m_positionTimer(new QTimer(this))
    // ★ 常驻一次，整个 controller 生命周期复用（见 m_toast 的说明）
    , m_toast(new bb::system::SystemToast(this))
    , m_openRequestTimer(new QTimer(this))
{
    // 轻提示固定在【屏幕底部】（默认在正中，会挡住封面和时间条）
    m_toast->setPosition(bb::system::SystemUiPosition::BottomCenter);

    /*
     * ★★ 启动后"推"封面几次。
     *   多任务视图封面的【第一次渲染会被吃掉】—— 实测：数据、路径全对，
     *   但只要处于静态（未在播放、或播放中还没出歌词），封面就是不显示；
     *   只要内容在变（例如歌词滚动）就正常。所以启动后先推几次，
     *   把首帧带出来（见 nudgeCover 的说明）。
     */
    QTimer::singleShot(1200, this, SLOT(nudgeCover()));
    QTimer::singleShot(2600, this, SLOT(nudgeCover()));
    QTimer::singleShot(4200, this, SLOT(nudgeCover()));

    // 延迟推页用的定时器：单次，到点才发 openRequestChanged（见 scheduleOpenRequest）
    m_openRequestTimer->setSingleShot(true);
    m_openRequestTimer->setInterval(200);
    connect(m_openRequestTimer, SIGNAL(timeout()), this, SLOT(onOpenRequestTimeout()));

    /*
     * 账号列表一变（登录 / 切换 / 删除 / 退出）就同步模型 ——
     * 这样各处入口不用每个都记得手动刷一遍。
     */
    connect(m_session, SIGNAL(accountsChanged()), this, SLOT(rebuildAccounts()));

    m_api->setHttpClient(m_http);
    m_api->setSession(m_session);

    connect(m_api, SIGNAL(searchFinished(int, nm::NmParsers::SearchParse)),
            this, SLOT(onSearchFinished(int, nm::NmParsers::SearchParse)));
    connect(m_api, SIGNAL(playlistFinished(int, nm::NmParsers::PlaylistParse)),
            this, SLOT(onPlaylistFinished(int, nm::NmParsers::PlaylistParse)));
    connect(m_api, SIGNAL(albumFinished(int, nm::NmParsers::PlaylistParse)),
            this, SLOT(onAlbumFinished(int, nm::NmParsers::PlaylistParse)));
    connect(m_api, SIGNAL(songUrlFinished(int, qint64, nm::NmParsers::SongUrlParse)),
            this, SLOT(onSongUrlFinished(int, qint64, nm::NmParsers::SongUrlParse)));
    connect(m_api, SIGNAL(accountFinished(int, nm::NmParsers::AccountParse)),
            this, SLOT(onAccountFinished(int, nm::NmParsers::AccountParse)));
    connect(m_api, SIGNAL(userPlaylistsFinished(int, nm::NmParsers::UserPlaylistsParse)),
            this, SLOT(onUserPlaylistsFinished(int, nm::NmParsers::UserPlaylistsParse)));
    connect(m_api, SIGNAL(songDetailFinished(int, nm::NmParsers::SongDetailParse)),
            this, SLOT(onSongDetailFinished(int, nm::NmParsers::SongDetailParse)));
    connect(m_api, SIGNAL(discoverFinished(int, nm::NmParsers::DiscoverParse)),
            this, SLOT(onDiscoverFinished(int, nm::NmParsers::DiscoverParse)));
    connect(m_api, SIGNAL(recommendFinished(int, nm::NmParsers::DiscoverParse)),
            this, SLOT(onRecommendFinished(int, nm::NmParsers::DiscoverParse)));
    connect(m_api, SIGNAL(commentsFinished(int, nm::NmParsers::CommentsParse)),
            this, SLOT(onCommentsFinished(int, nm::NmParsers::CommentsParse)));
    connect(m_api, SIGNAL(levelFinished(int, nm::NmParsers::LevelParse)),
            this, SLOT(onLevelFinished(int, nm::NmParsers::LevelParse)));
    connect(m_api, SIGNAL(userDetailFinished(int, nm::NmParsers::UserDetailParse)),
            this, SLOT(onUserDetailFinished(int, nm::NmParsers::UserDetailParse)));
    connect(m_api, SIGNAL(artistsFinished(int, nm::NmParsers::ArtistsParse)),
            this, SLOT(onArtistsFinished(int, nm::NmParsers::ArtistsParse)));
    connect(m_api, SIGNAL(mvsFinished(int, nm::NmParsers::MvsParse)),
            this, SLOT(onMvsFinished(int, nm::NmParsers::MvsParse)));
    connect(m_api, SIGNAL(mvUrlFinished(int, nm::NmParsers::MvUrlParse)),
            this, SLOT(onMvUrlFinished(int, nm::NmParsers::MvUrlParse)));

    connect(m_api, SIGNAL(requestFailed(int, QString, QString, QVariantMap)),
            this, SLOT(onRequestFailed(int, QString, QString, QVariantMap)));

    /*
     * 封面下载完成 → 版本号 +1（触发页面级绑定）+ 排一次合并的
     * 模型刷新（80ms 窗口把同时完成的下载合并成一次全表扫描，
     * 一次搜索 30 首歌的封面陆续下完时不至于刷 30 遍列表）。
     */
    connect(m_imageCache, SIGNAL(ready()), this, SLOT(onImageReady()));

    /*
     * 大图缓存（只服务正在播放页的封面，见 bigImagePath）：
     *   640px —— 播放页封面按 76.8du 显示（Z10 上 ≈246px），留一倍余量
     *            给"点开列表放大""换更大屏"这类情况；再大只是白占磁盘。
     *   并发 1 —— 播放页同一时刻只需要一张封面，没必要跟列表抢带宽。
     * ★ ready() 不接 onImageReady（那会顺带排一次全模型扫描，白扫），
     *   走 onBigImageReady：只顶版本号。
     */
    m_bigImageCache->setMaxImageSize(640);
    m_bigImageCache->setMaxConcurrent(1);
    connect(m_bigImageCache, SIGNAL(ready()), this, SLOT(onBigImageReady()));

    /*
     * ★ 一上来就把消息处理器装好（默认关闭）。
     *   不能只在 setConsoleLogEnabled() 里装 —— 那样在用户【第一次拨开关之前】
     *   根本没有处理器，Qt 会用默认处理器把日志照常全打出来，
     *   表现就是"开关没生效"。
     */
    setConsoleLogEnabled(consoleLogEnabled());

    m_imageRefreshTimer->setSingleShot(true);
    m_imageRefreshTimer->setInterval(80);
    connect(m_imageRefreshTimer, SIGNAL(timeout()), this, SLOT(refreshImagePaths()));

    /*
     * ★ 搜索防抖定时器：QTimer 必须在这里 new，
     *   之前那次写入没落盘，m_searchTimer 成了野指针，
     *   search() 里 start() 直接崩（和 m_browseSongs 同一个坑）。
     *   连续输入时 start() 会重置计时，只发最后一次请求。
     */
    m_searchTimer = new QTimer(this);
    m_searchTimer->setSingleShot(true);
    m_searchTimer->setInterval(300);
    connect(m_searchTimer, SIGNAL(timeout()), this, SLOT(onSearchDebounce()));

    // 音频落盘缓存（播放的同时后台缓存，见 NmAudioCache）
    m_audioCache = new nm::NmAudioCache(this);

    // 歌词开关 / 翻译开关：从设置里恢复
    {
        QSettings settings;
        m_lyricsEnabled = settings.value(QLatin1String("nm/lyrics"), false).toBool();
        m_lyricsTransEnabled =
            settings.value(QLatin1String("nm/lyricsTrans"), false).toBool();
        // 歌词没开时翻译不该是开着的（历史上可能存过这种组合，顺手修正）
        if (!m_lyricsEnabled && m_lyricsTransEnabled) {
            m_lyricsTransEnabled = false;
            settings.setValue(QLatin1String("nm/lyricsTrans"), false);
        }
    }

    /*
     * ★★ 歌词接口的信号连接 —— 之前这条写入没落盘，
     *   onLyricFinished 永远收不到回调，歌词根本拉不下来。
     */
    connect(m_api, SIGNAL(lyricFinished(int, nm::NmParsers::LyricParse)),
            this, SLOT(onLyricFinished(int, nm::NmParsers::LyricParse)));

    /*
     * ★★ 构建标记（诊断用）。
     *   直接 fprintf 到 stderr，【不经过 qWarning】，所以不会被"输出控制台
     *   日志"开关吞掉、也不会被消息处理器过滤 —— 只要启动就一定打印。
     *
     *   用途：确认设备上跑的到底是不是最新二进制。
     *   如果日志里【看不到】这一行，说明装的是旧包，改多少代码都没用。
     */
    fprintf(stderr, "[BUILD-STAMP] MusicController lyrics=1 trans=1 browse=1 audiocache=1\n");
}

MusicController::~MusicController()
{
}

ArrayDataModel *MusicController::songs() const
{
    return m_songs;
}

void MusicController::init()
{
    m_session->load();

    // 读「隐藏 VIP 歌曲」设置（默认关）
    QSettings hiddenSettings;
    m_hideVip = hiddenSettings.value(QLatin1String("nm/hideVip"), false).toBool();

    /*
     * ★★ 内置开发凭证【已移除】（发版前清理）。
     *
     *   原来这里在没有本地登录态时，会用 NmDevCredentials.hpp 里的
     *   kDevMusicU 自动登录，方便模拟器调试（模拟器没法把长字符串
     *   粘进输入框）。但那等于把一份 MUSIC_U —— 也就是账号的完整
     *   登录凭证 —— 明文写进仓库，属于必须清掉的敏感信息。
     *
     *   现在要在模拟器上登录，走正路：
     *     - 真实设备：登录页 →「粘贴」（从剪贴板读 MUSIC_U / 整段 cookie）
     *     - 模拟器：同一条路（剪贴板可以先由外部塞进去），
     *               登录一次后凭证存在应用沙箱的 QSettings 里，后续启动复用
     *   ⚠ 如果这份 MUSIC_U 曾经进过任何公开的地方（仓库历史/截图/聊天），
     *     请到网易云网页端退出登录再重新登录，让它【失效】。
     */

    // 我的歌单先显示本地缓存（不用等网络，也不用手点刷新）
    loadPlaylistCache();

    // 推荐歌单 / MV / 歌手 同样先用本地缓存顶上，联网后再刷新
    loadDiscoverCache();
    loadMvCache();
    loadArtistsCache();

    // 账号列表先按磁盘上的内容铺一遍（不用等联网校验）
    rebuildAccounts();
    /*
     * 「全部歌曲」的本地缓存 + 由它算出来的「艺术家」索引，开机就铺好。
     *
     * ★★ 原来这份缓存只在点「全部歌曲」Tab 时才加载（见 loadAllSongs），
     *    所以只要没进过那个 Tab，资料库的「艺术家」就是空的（真机反馈）。
     *    loadAllSongsCache() 内部会 rebuildAllSongs()，顺带重建艺术家索引。
     */
    loadAllSongsCache();
    rebuildLibraryArtists();    // 缓存为空时也走一遍，把模型清干净

    // 有凭证就校验一次登录态（结果异步到达，成功后会自动刷新歌单和我的页）
    if (m_session->hasCookie())
        m_api->fetchAccount();
}

void MusicController::login(const QString &cookieInput)
{
    if (!m_session->setCookieInput(cookieInput)) {
        m_lastLoginError = NmTr("E6B2A1E69C89E4BB8EE8BE93E585A5E4B8ADE689BEE588B04D555349435F55");
        emit lastLoginErrorChanged();
        return;
    }

    m_lastLoginError.clear();
    emit lastLoginErrorChanged();

    /*
     * ★ 校验期间把 loading 打开 —— 登录页的「正在校验登录态…」绑的就是它。
     *   必须保证这条 loading 会关掉：
     *     成功 → onAccountFinished 里接着发的那串 fetch*（等级/歌单/关注艺人）
     *            各自 finishRequest 收尾；
     *     失败 → onAccountFinished 的失败分支直接 setLoading(false)。
     *   漏掉的话，粘了一份失效凭证后登录页会一直转圈。
     */
    setLoading(true);
    m_api->fetchAccount();
}

void MusicController::logout()
{
    m_session->clear();
    m_loggedIn = false;
    m_nickName.clear();
    m_userId = 0;
    m_favoritePlaylistId.clear();
    emit loginChanged();
}

// ===================== 账号管理（多账号） =====================

QVariantMap MusicController::currentAccount() const
{
    return m_session->currentAccount();
}

int MusicController::otherAccountCount() const
{
    return m_otherAccounts->size();
}

void MusicController::rebuildAccounts()
{
    m_otherAccounts->clear();
    foreach (const QVariant &item, m_session->otherAccounts()) {
        QVariantMap row = item.toMap();
        // 头像下到本地的路径带一份，列表 delegate 就不用各自去求
        row.insert(QLatin1String("localAvatarPath"),
                   m_imageCache->pathFor(row.value(QLatin1String("avatarUrl")).toString()));
        m_otherAccounts->append(row);
    }

    ++m_accountsVersion;
    emit accountsChanged();
}

bool MusicController::switchAccount(const QString &id)
{
    if (! m_session->switchTo(id))
        return false;

    /*
     * 换了凭证：先把上一个账号的数据清干净，再重新校验。
     * 校验回来（onAccountFinished）会自动拉新账号的歌单和"我的"页。
     * ★ 必须清，否则界面上会短暂顶着上一个账号的昵称/歌单。
     */
    m_loggedIn = false;
    m_nickName.clear();
    m_userId = 0;
    m_userAvatarUrl.clear();
    m_favoritePlaylistId.clear();
    m_playlists->clear();
    ++m_playlistsVersion;
    emit playlistsChanged();
    emit loginChanged();

    setLoading(true);
    m_api->fetchAccount();
    return true;
}

bool MusicController::removeAccount(const QString &id)
{
    // 先看删的是不是当前在用的（删完就查不出来了）
    const bool wasCurrent =
        m_session->currentAccount().value(QLatin1String("id")).toString() == id;

    if (! m_session->removeAccount(id))
        return false;

    if (wasCurrent) {
        // 凭证已经被 session 清掉了，这里把内存里的登录态也收干净
        m_loggedIn = false;
        m_nickName.clear();
        m_userId = 0;
        m_userAvatarUrl.clear();
        m_favoritePlaylistId.clear();
        m_playlists->clear();
        ++m_playlistsVersion;
        emit playlistsChanged();
        emit loginChanged();
    }
    return true;
}

QString MusicController::accountCredential(const QString &id) const
{
    return m_session->credential(id);
}

void MusicController::requestOpenLogin()
{
    m_openRequestKind = QLatin1String("login");
    m_openRequestArg.clear();
    scheduleOpenRequest();
}

void MusicController::search(const QString &keyword)
{
    const QString text = keyword.trimmed();
    if (text.isEmpty()) {
        setError(NmTr("E8AFB7E58588E8BE93E585A5E585B3E994AEE8AF8D"));
        return;
    }

    /*
     * ★ 防抖：搜索框每敲一个字都会调到这里（页面里满 2 字就发），
     *   连打 "closer" 就是 5 次请求 —— 高频请求很容易触发服务端限流、
     *   返回空结果（真机：每次都 94 字节、一首都没有）。
     *   这里只暂存关键词并重置定时器，停手 300ms 后才真正发请求。
     */
    m_pendingSearch = text;
    m_searchTimer->start(300);
}

void MusicController::onSearchDebounce()
{
    const QString text = m_pendingSearch.trimmed();
    if (text.isEmpty())
        return;

    /*
     * ★ 浏览模式：【不调 clearSongs()】—— 那个会清空全局 m_songs（= 播放队列）。
     *   搜索结果填进独立浏览模型，只有点某首播放（playBrowseSong）才接管队列。
     */
    m_browseMode = true;
    m_artistMode = false;
    m_recommendMode = false;
    clearBrowseList();      // 加载前清空浏览列表，避免残留上一次的搜索结果

    setLoading(true);
    m_lastQuery = text;
    m_playlistInfo.clear();      // 搜索结果不是歌单，标题改用关键词
    m_api->searchSongs(text, 30);
}

void MusicController::loadPlaylist(const QString &playlistId)
{
    const QString text = playlistId.trimmed();
    if (text.isEmpty()) {
        setError(NmTr("E8AFB7E58588E8BE93E585A5E6AD8CE58D954944"));
        return;
    }

    /*
     * ★ 先清掉上一个歌单的信息与关键词。
     *   否则切歌单时（网络慢 / 命中缓存期间）标题栏会残留上一个歌单的名字：
     *   listTitle 的 NOTIFY 是 songsChanged，而下面 clearSongs() 会触发它，
     *   必须在那之前把 m_playlistInfo / m_lastQuery 清干净。
     */
    m_lastQuery.clear();
    m_playlistInfo.clear();
    ++m_playlistInfoVersion;
    emit playlistInfoChanged();

    /*
     * ★ 浏览模式：【不调 clearSongs()】—— 那个会清空全局 m_songs（= 播放队列）。
     *   「打开歌单看看」不该动队列；曲目填进独立浏览模型，
     *   只有点某首播放（playBrowseSong）才把它拷成队列。
     */
    m_browseMode = true;
    m_artistMode = false;
    m_recommendMode = false;
    /*
     * ★ 加载前清【浏览列表】（不是队列）：不清的话网络回来前会短暂
     *   显示上一个歌单的内容。原来靠 clearSongs() 兼任，但它会清队列。
     */
    clearBrowseList();

    // 资料库是高频访问：先显示本地缓存的曲目，网络结果回来再刷新
    m_allSongsMode = false;   // 普通歌单加载：确保不误走「全部歌曲」分流
    m_pendingPlaylistId = text;
    const bool fromCache = loadPlaylistSongsCache(text);

    // 命中缓存就不转圈：先显示缓存内容，后台静默刷新
    // （之前无条件 setLoading(true) 会盖个转圈，看着像"每次都要重新加载"）
    setLoading(! fromCache);
    m_api->fetchPlaylist(text);
}

void MusicController::loadMyPlaylists()
{
    if (!m_session->hasCookie()) {
        setError(NmTr("E8AFB7E58588E799BBE5BD95E5908EE5868DE6999AE79C8BE6AD8CE58D95"));
        return;
    }

    // 去重：正在拉就不重复发（避免连点/多次导航打一堆一样的请求）
    if (m_playlistsFetching)
        return;
    m_playlistsFetching = true;

    if (m_userId > 0) {
        setLoading(true);
        m_api->fetchUserPlaylists(m_userId);
        return;
    }

    // uid 还没拿到：先走一次账号校验，回来后自动拉
    m_pendingPlaylistsLoad = true;
    setLoading(true);
    m_api->fetchAccount();
}

QString MusicController::favoritePlaylistId() const
{
    return m_favoritePlaylistId;
}

void MusicController::requestOpenAlbum(const QString &albumId, const QString &name)
{
    Q_UNUSED(name);   // 专辑名由接口返回，这里只带 id
    m_openRequestKind = QLatin1String("album");
    m_openRequestArg = albumId;
    // ★ 不在这里直接发信号，推迟 200ms 再通知 QML 推页（见 scheduleOpenRequest）
    scheduleOpenRequest();
}

void MusicController::loadAlbum(const QString &albumId)
{
    const QString text = albumId.trimmed();
    if (text.isEmpty())
        return;

    // ★ 同上：停掉待发的搜索防抖，避免它把模式标志冲掉
    if (m_searchTimer)
        m_searchTimer->stop();

    // 和歌单一样：先清标题，避免残留上一个列表的名字
    m_lastQuery.clear();
    m_playlistInfo.clear();
    ++m_playlistInfoVersion;
    emit playlistInfoChanged();

    // ★ 同上：专辑也只是浏览，填独立浏览模型，【不动播放队列】
    m_browseMode = true;
    m_artistMode = false;
    m_recommendMode = false;
    clearBrowseList();

    setLoading(true);
    m_api->fetchAlbum(text.toLongLong());
}

void MusicController::playBrowseSong(int index)
{
    /*
     * ★ 用【显示的模型】而不是完整 maps：
     *   打开「隐藏 VIP 歌曲」时 m_browseSongs 比 m_browseSongMaps 少几条，
     *   两个下标空间就不再一一对应 —— 按 maps 取会播成别的歌。
     *   （照 playArtistSong / playRecommendSong 的写法。）
     */
    if (index < 0 || index >= m_browseSongs->size())
        return;

    QVariantList shown;
    for (int i = 0; i < m_browseSongs->size(); ++i)
        shown.append(m_browseSongs->value(i));

    /*
     * 只有这一步才把浏览列表变成播放队列。
     * 在此之前列表安静躺在 m_browseSongs，全局 m_songs（队列）没被动过。
     * setPlaylistFrom() 会拷一份过去并置 m_queueAdopted（返回时不再还原）。
     */
    setPlaylistFrom(shown);
    playIndex(index);
}

void MusicController::requestCommentsForBrowseIndex(int index)
{
    // ★ 同上：按【显示模型】取，隐藏 VIP 时下标才对得上
    if (index < 0 || index >= m_browseSongs->size())
        return;

    const qint64 songId = m_browseSongs->value(index).toMap()
                              .value(QLatin1String("id")).toString().toLongLong();
    if (songId <= 0)
        return;

    /*
     * ★ 必须置 autoOpen：否则评论是拉回来了，但不会自动推评论页
     *   （用户看到的现象就是"长按查看评论，评论区弹不出来"）。
     */
    m_commentsAutoOpen = true;
    m_commentsTitle = NmTr("E6AD8CE69BB2E8AF84E8AEBA");   // 歌曲评论
    clearComments();
    setLoading(true);
    m_api->fetchComments(songId, 30, 0);
}

/*
 * 搜索页专用的两个入口。
 * ★ 搜索页显示的是【独立】的 music.searchSongs —— 它只是"搜索那一刻"的快照。
 *   用户之后打开歌单/专辑会把浏览列表整个换掉，这时再按 browse 下标取数
 *   就会取到完全不相干的歌（真机反馈："搜索页 item 的菜单行为不对"）。
 *   所以这两个都直接基于 m_searchSongs 取数。
 */
void MusicController::playSearchSong(int index)
{
    if (index < 0 || index >= m_searchSongs->size())
        return;

    QVariantList shown;
    for (int i = 0; i < m_searchSongs->size(); ++i)
        shown.append(m_searchSongs->value(i));

    setPlaylistFrom(shown);
    playIndex(index);
}

void MusicController::requestCommentsForSearchIndex(int index)
{
    if (index < 0 || index >= m_searchSongs->size())
        return;

    const qint64 songId = m_searchSongs->value(index).toMap()
                              .value(QLatin1String("id")).toString().toLongLong();
    if (songId <= 0)
        return;

    // ★ 同上：不置 autoOpen 的话评论页不会弹出来
    m_commentsAutoOpen = true;
    m_commentsTitle = NmTr("E6AD8CE69BB2E8AF84E8AEBA");   // 歌曲评论
    clearComments();
    setLoading(true);
    m_api->fetchComments(songId, 30, 0);
}

// ==================== 音乐详情（Properties） ====================

/*! 是不是「作词 / 作曲 / Composer…」这类职员表行（完整实现见下方净化一节） */
static bool isLyricProductionLine(const QString &text);

/*!
 * 从歌词原文开头提取职员表，供「音乐详情」页展示。
 * ★ 和净化用的是【同一张关键词表】：
 *   净化负责把这些行从"歌词显示"里摘掉，这里负责把它们"读出来"。
 */
static QList<QPair<QString, QString> > extractLyricCredits(const QString &raw)
{
    QList<QPair<QString, QString> > out;
    if (raw.isEmpty())
        return out;

    const QStringList lines = raw.split(QLatin1Char('\n'));
    const int max = qMin(lines.size(), 40);
    for (int i = 0; i < max; ++i) {
        const QString line = lines.at(i);

        // 跳过行首的 [00:00.00] 时间标签
        int pos = 0;
        while (pos < line.length()) {
            const int lb = line.indexOf(QLatin1Char('['), pos);
            if (lb < 0)
                break;
            const int rb = line.indexOf(QLatin1Char(']'), lb + 1);
            if (rb < 0)
                break;
            pos = rb + 1;
        }
        const QString text = line.mid(pos).trimmed();
        if (text.isEmpty())
            continue;

        int sep = text.indexOf(QLatin1Char(':'));
        if (sep < 0)
            sep = text.indexOf(QString::fromUtf8("："));
        if (sep <= 0)
            continue;

        const QString label = text.left(sep).trimmed();
        const QString value = text.mid(sep + 1).trimmed();
        if (label.isEmpty() || value.isEmpty() || label.length() > 40)
            continue;
        if (!isLyricProductionLine(text))
            continue;

        out.append(qMakePair(label, value));
        if (out.size() >= 8)      // 详情页不需要几十行职员表
            break;
    }
    return out;
}

QVariantList MusicController::songProperties() const
{
    QVariantList out;
    /*
     * ★★ 读的是【详情目标】那首歌，不是"正在播放"那首！
     *   这里之前一直是 m_currentTrack —— 从"全部歌曲/歌单"长按进详情时，
     *   看的是正在播放那首（甚至没在播就整个空掉，显示"当前没有播放中的歌曲"）。
     */
    if (m_propertiesTrack.isEmpty()
            || m_propertiesTrack.value(QLatin1String("name")).toString().isEmpty())
        return out;

    QVariantMap row;
    row.insert(QLatin1String("title"), QString::fromUtf8("歌曲名"));
    row.insert(QLatin1String("description"),
               m_propertiesTrack.value(QLatin1String("name")));
    out.append(row);

    row.clear();
    row.insert(QLatin1String("title"), QString::fromUtf8("作者"));
    row.insert(QLatin1String("description"),
               m_propertiesTrack.value(QLatin1String("artistsText")));
    out.append(row);

    row.clear();
    row.insert(QLatin1String("title"), QString::fromUtf8("ID"));
    row.insert(QLatin1String("description"),
               m_propertiesTrack.value(QLatin1String("id")));
    out.append(row);

    row.clear();
    row.insert(QLatin1String("title"), QString::fromUtf8("长度"));
    row.insert(QLatin1String("description"),
               m_propertiesTrack.value(QLatin1String("durationText")));
    out.append(row);

    row.clear();
    row.insert(QLatin1String("title"), QString::fromUtf8("所属专辑"));
    row.insert(QLatin1String("description"),
               m_propertiesTrack.value(QLatin1String("albumName")));
    out.append(row);

    // 作词 / 作曲 / 编曲 …：从【详情目标那份】歌词原文读，没有就不显示
    const QList<QPair<QString, QString> > credits = extractLyricCredits(m_propertiesLyric);
    for (int i = 0; i < credits.size(); ++i) {
        QVariantMap c;
        c.insert(QLatin1String("title"), credits.at(i).first);
        c.insert(QLatin1String("description"), credits.at(i).second);
        out.append(c);
    }

    return out;
}

QString MusicController::propertiesLyric() const
{
    return m_propertiesLyric;
}

void MusicController::enqueueAlbum(const QString &albumId)
{
    const QString text = albumId.trimmed();
    if (text.isEmpty())
        return;

    // 标记为"追加模式"，onAlbumFinished 里据此走另一条分支
    m_enqueueAlbumMode = true;
    setLoading(true);
    m_api->fetchAlbum(text.toLongLong());
}

void MusicController::onAlbumFinished(int requestId,
                                      const nm::NmParsers::PlaylistParse &result)
{
    Q_UNUSED(requestId);

    if (!result.ok) {
        setError(result.error);
        m_enqueueAlbumMode = false;
        finishRequest(QLatin1String("album"), false);
        return;
    }

    /*
     * ★ 追加模式（艺人页「增加到队列」）：把曲目插到当前曲目【之后】，
     *   不碰 m_playlistInfo、也不清空当前列表 —— 和下面"打开专辑页"那条路
     *   完全分开，避免把用户正在看/播的内容顶掉。
     */
    if (m_enqueueAlbumMode) {
        m_enqueueAlbumMode = false;

        const bool indexOk = m_currentIndex >= 0 && m_currentIndex < m_songs->size();
        int at = indexOk ? (m_currentIndex + 1) : m_songs->size();

        foreach (const nm::NmSong &song, result.songs) {
            QVariantMap map = song.toVariantMap();
            map.insert(QLatin1String("localArtPath"),
                       m_imageCache->pathFor(song.artUrl));
            map.insert(QLatin1String("type"), QLatin1String("song"));
            m_songs->insert(at++, map);
            m_songMaps.append(map);
        }

        emit songsChanged();
        notifyError(QString::fromUtf8("已加入队列 %1 首").arg(result.songs.size()));
        finishRequest(QLatin1String("album"), true);
        return;
    }

    // 先设专辑信息再填曲目（同 onPlaylistFinished 的顺序要求）
    m_playlistInfo = result.info.toVariantMap();
    ++m_playlistInfoVersion;
    emit playlistInfoChanged();

    fillSongsModel(result.songs);
    emit songsChanged();

    finishRequest(QLatin1String("album"), true);
}

QString MusicController::imagePath(const QString &url)
{
    return m_imageCache->pathFor(url);
}

QString MusicController::coverArtPath(const QString &url)
{
    const QString path = bigImagePath(url);

    /*
     * 限量诊断：只在 (url, 结果) 真的变化时打一条。
     *   日志一条都不出现     → cover 的绑定压根没求值（content 没渲染出来）
     *   一直打 (未就绪)      → 640px 大图还在下载（"卡几秒才显示"多半是它）
     *   从 (未就绪) → 本地路径 → 图下好后绑定也重算了（这条链路是通的）
     */
    ++m_coverEvalCount;
    const QString key = url + QLatin1Char('|') + path;
    if (key != m_coverDiagLast) {
        m_coverDiagLast = key;
        qWarning("MusicController: cover eval #%d url=%s -> %s",
                 m_coverEvalCount,
                 qPrintable(url.right(44)),
                 path.isEmpty() ? "(未就绪)" : qPrintable(path.right(44)));
    }
    return path;
}

// ============================================================================
//  多任务视图封面（SceneCover）要用的数据
//
//  ★ 全部是【基本类型】属性（QString / bool），QML 那边只做 `music.xxx`
//    这种最普通的读取 —— 不写多行 JS，也不用 QVariantMap（map 属性每次
//    求值都返回新对象，在 cover 的独立上下文里是可疑的不稳定因素）。
// ============================================================================

void MusicController::notifyCoverAgain()
{
    emit coverInfoChanged();
}

void MusicController::nudgeCover()
{
    /*
     * ★★★ 起播后"轻轻推"封面几次。
     *
     *   实测（真机日志 + 用户复现）：数据链路完全正确 ——
     *     路径对、共享目录对、coverImagePath 也按预期从 PLACEHOLDER 变成 REAL，
     *   但封面就是不显示；而一旦歌词开始滚动（每句都在变），封面立刻出现。
     *
     *   结论：SceneCover 需要【内容层面】的变化才会重绘；单纯改 imageSource
     *   的值喂不饱它。没歌词的歌、以及未在播放，都没有后续变化，所以封面永远
     *   画不出来。
     *
     *   所以这里在起播后隔一段时间推几次：只把 m_coverNudge 递增，让 coverTitle
     *   交替带上一个【零宽空格】（肉眼不可见、不影响排版），借这一次"内容变化"
     *   把封面顶出来。跑几次就够，之后不再打扰。
     */
    ++m_coverNudge;
    emit coverInfoChanged();
}

QString MusicController::coverRealPath()
{
    if (m_currentTrack.isEmpty())
        return QString();

    const QString url = m_currentTrack.value(QLatin1String("artUrl")).toString();
    if (url.isEmpty())
        return QString();

    /*
     * 640px 大图优先 → 160px 小图；都没有返回空串，QML 那边自己铺
     * 「深色底 + 居中白色音符」。
     *
     * ★ 绝对【不要】在这里兜底成 ic_default.png —— 那张图是深灰底加黑音符，
     *   AspectFill 铺满之后看着就是一片黑，会被当成 bug（实测踩过）。
     *
     * 走 coverArtPath 是为了保留它那条限量诊断（能看出绑定在不在求值）。
     */
    QString path = coverArtPath(url);
    if (path.isEmpty())
        path = m_imageCache->pathFor(url);  // 160px，列表那份，通常已就绪

    /*
     * ★★ 只把【确实存在、且非空】的文件交给 ImageView。
     *
     *   实测线索：起播后封面图还在往磁盘写的那个瞬间，把路径交给 ImageView
     *   会让它整片画成【黑色】（不是透明！）—— 正好把底层的兜底底纹盖死，
     *   多任务视图就成了一块纯黑；等歌词到达、别的绑定重算让 content 重建
     *   之后才恢复（真机日志：title=I Miss You 时全黑，title=歌词 时正常）。
     *
     *   所以这里先确认文件可用；不可用就返回空串，让底层兜底露出来 ——
     *   宁可先显示"深色底 + 音符"，也不要一块黑。
     *
     *   （日志里的 exists/size 是诊断，用来确认这条链路。）
     */
    if (! path.isEmpty()) {
        /*
         * ★★ path 是【file:/// 开头的 URL】（NmImageCache 就是这么给的），
         *   而 QFile 只吃本地路径 —— 不剥掉前缀的话 exists() 永远是 false，
         *   于是封面会被误判成"没准备好"，永远只显示兜底图（实测踩过）。
         */
        QString local = path;
        if (local.startsWith(QLatin1String("file://")))
            local = local.mid(7);

        QFile f(local);
        if (! f.exists() || f.size() <= 0) {
            qWarning("MusicController: cover file NOT ready "
                     "(exists=%d size=%lld) %s",
                     f.exists() ? 1 : 0, (long long) f.size(),
                     qPrintable(path.right(44)));
            return QString();
        }
    }
    return path;
}

bool MusicController::coverHasArt()
{
    return ! coverRealPath().isEmpty();
}

/*
 * 把一张本地图片复制一份到【共享目录】，返回共享目录里的本地路径（失败给空串）。
 *
 * ★★ 为什么必须这么做（踩了很久）：
 *   多任务视图封面（SceneCover / Active Frame）的内容是由【系统服务】渲染的，
 *   它读不到应用私有沙箱 ——
 *     /accounts/1000/appdata/<app>/data/nm-img/xxxx-s640.jpg
 *   这种路径它读不出来。表现就是：ImageView 拿到这个 file:// 之后什么都不画，
 *   而 asset:///images/ic_default.png 却正常（那是打包进应用包的资源，系统读得到）。
 *   于是"未在播放正常、一播放封面就黑"。
 *
 *   共享目录 /accounts/1000/shared/ 是系统服务访问得到的（需要 access_shared
 *   权限，见 bar-descriptor.xml）。
 *
 * 文件名沿用源文件名（本身是 MD5+尺寸，天然唯一），已复制过就不重复拷。
 */
static QString sharedCoverFor(const QString &localPath)
{
    const QFileInfo src(localPath);
    if (! src.exists() || src.size() <= 0)
        return QString();

    const QString dir = QLatin1String("/accounts/1000/shared/documents/nm-cover");
    QDir d;
    if (! d.mkpath(dir)) {
        qWarning("MusicController: cannot create shared cover dir %s", qPrintable(dir));
        return QString();
    }

    const QString dst = dir + QLatin1Char('/') + src.fileName();
    const QFileInfo dstInfo(dst);
    if (dstInfo.exists() && dstInfo.size() == src.size())
        return dst;                     // 已经拷过了

    QFile::remove(dst);                 // 大小不一致（或残留）先清掉
    if (! QFile::copy(localPath, dst)) {
        qWarning("MusicController: cover copy FAILED src=%s dst=%s",
                 qPrintable(localPath.right(48)), qPrintable(dst.right(48)));
        return QString();
    }
    qWarning("MusicController: cover copied to shared: %s", qPrintable(dst.right(48)));
    return dst;
}

QString MusicController::coverImagePath()
{
    // 兜底图：asset:// 是打包进应用包的资源，系统服务读得到，永远不会是黑屏
    static const QString kFallback = QLatin1String("asset:///images/ic_default.png");

    const QString real = coverRealPath();
    if (real.isEmpty()) {
        if (m_coverDiagPath != QLatin1String("EMPTY")) {
            m_coverDiagPath = QLatin1String("EMPTY");
            qWarning("MusicController: coverImagePath -> EMPTY (无封面，给兜底图)");
        }
        return kFallback;
    }

    // file:// → 本地路径
    QString local = real;
    if (local.startsWith(QLatin1String("file://")))
        local = local.mid(7);

    /*
     * 复制到共享目录再交给 ImageView —— 它读不到应用沙箱（见 sharedCoverFor
     * 上面的说明）。拷不过去就退回兜底图，宁可显示占位也不要一块黑。
     */
    const QString shared = sharedCoverFor(local);
    if (shared.isEmpty()) {
        qWarning("MusicController: cover -> FALLBACK (real=%s)",
                 qPrintable(real.right(48)));
        return kFallback;
    }

    /*
     * ★★★ 关键一步：【先给占位，再切真图】—— 故意制造一次值变化。
     *
     *   实测：SceneCover 会"吃掉"第一次渲染。起播时就算 imageSource 直接是
     *   真封面，封面服务那一次也画不出来；只有等到后续有内容变化（比如歌词
     *   到达、或者切歌）才会补上。这就解释了真机现象：
     *     · 有歌词   → 歌词每句都在变，不断触发重绘 → 正常 ✓
     *     · 没歌词   → 起播后再没有变化 → 一直黑 ✗
     *     · 未在播放 → 完全没有变化 → 黑 ✗
     *
     *   所以这里第一次求值先返回 asset:// 占位（打包资源，任何时刻都画得出来），
     *   然后推迟一下再发通知 —— 那时 m_coverReady 已置位，返回值变成共享目录里
     *   的真封面，imageSource 就有了 asset:// → file:// 的【真实值变化】，
     *   逼系统重绘一次。
     */
    if (! m_coverReady) {
        m_coverReady = true;
        m_coverDiagPath = QLatin1String("PLACEHOLDER");
        qWarning("MusicController: coverImagePath -> PLACEHOLDER (第一步：占位)");
        QTimer::singleShot(80, this, SLOT(notifyCoverAgain()));
        return kFallback;
    }

    const QString out = QUrl::fromLocalFile(shared).toString();
    if (m_coverDiagPath != out) {
        m_coverDiagPath = out;
        qWarning("MusicController: coverImagePath -> REAL %s", qPrintable(out.right(52)));
    }
    return out;
}

QString MusicController::coverTitle()
{
    QString t;

    if (m_currentTrack.isEmpty()) {
        t = NmTr("E69CAAE59CA8E692ADE694BE");           // 未在播放
    } else if (m_lyricsEnabled && ! m_currentLyricLine.isEmpty()) {
        t = m_currentLyricLine;                          // 歌词（开着且出词了）
    } else {
        t = m_currentTrack.value(QLatin1String("name")).toString();   // 歌名
    }

    /*
     * ★ 奇数时追加一个【零宽空格】（U+200B）：肉眼不可见、不占宽度、不影响
     *   排版，但能让这个值发生变化 —— 借这次"内容变化"驱动 SceneCover 重绘。
     *   背景见 nudgeCover 的说明：静态状态下封面永远画不出来。
     */
    if (m_coverNudge % 2)
        t += QChar(0x200B);

    return t;
}

QString MusicController::coverSubtitle()
{
    if (m_currentTrack.isEmpty())
        return QString();

    const QString name = m_currentTrack.value(QLatin1String("name")).toString();
    const QString artists =
        m_currentTrack.value(QLatin1String("artistsText")).toString();

    // 出词时：「翻译」或「歌名 - 作者」；没出词：作者
    if (m_lyricsEnabled && ! m_currentLyricLine.isEmpty()) {
        if (m_lyricsTransEnabled && ! m_currentLyricTrans.isEmpty())
            return m_currentLyricTrans;
        return name + QLatin1String(" - ") + artists;
    }
    return artists;
}

QString MusicController::bigImagePath(const QString &url)
{
    /*
     * 和 imagePath 唯一的区别：走大图缓存（640px）。
     * 两边文件名带尺寸后缀，同一张封面在磁盘上会各存一份（小图给小列表、
     * 大图给播放页），互不影响。
     */
    if (!m_bigImageCache)
        return QString();

    return m_bigImageCache->pathFor(url);
}

void MusicController::onBigImageReady()
{
    // 大图不进模型字段，只让 QML 的绑定重新求值（见 hpp 里的说明）
    ++m_imageCacheVersion;
    emit imageCacheChanged();
    emit coverInfoChanged();
}

void MusicController::loadDiscover()
{
    setLoading(true);
    m_api->fetchDiscoverPlaylists(20);
}

void MusicController::loadRecommendSongs()
{
    if (!m_session->hasCookie()) {
        setError(NmTr("E6AF8FE697A5E68EA8E88D90E99C80E8A681E799BBE5BD95")); // 每日推荐需要登录
        return;
    }

    /*
     * ★★ 切到【推荐模式】并清掉上一次的推荐内容。
     *
     *   原来这里只调了 clearSongs() —— 那清的是【全局播放队列】，
     *   既没切模式、又平白把队列清了；而推荐页绑的是独立模型
     *   music.recommendSongs。填错模型 + 队列被清，真机表现就是
     *   "推荐页一直显示『登录以加载推荐』"。
     */
    m_recommendMode = true;
    m_artistMode = false;
    m_browseMode = false;

    m_imageCache->cancelQueued();
    m_recommendSongs->clear();
    m_recommendSongMaps.clear();
    emit recommendSongsChanged();      // 让页面立刻变空，别残留上一次的推荐

    setLoading(true);
    m_api->fetchRecommendSongs();
}

void MusicController::loadCommentsForCurrent()
{
    if (m_currentIndex < 0 || m_currentIndex >= m_songMaps.size()) {
        setError(NmTr("E58588E692ADE694BEE4B880E9A696E6AD8CE69BB2"));   // 先播放一首歌曲
        return;
    }

    const qint64 songId =
        m_currentTrack.value(QLatin1String("id")).toString().toLongLong();
    if (songId <= 0) {
        setError(NmTr("E58588E692ADE694BEE4B880E9A696E6AD8CE69BB2"));   // 先播放一首歌曲
        return;
    }

    m_commentsTitle = NmTr("E6AD8CE69BB2E8AF84E8AEBA");   // 歌曲评论

    clearComments();
    setLoading(true);
    m_api->fetchComments(songId, 30, 0);
}

void MusicController::clearComments()
{
    /*
     * ★★ 请求一开始就清空，而不是等结果回来再覆盖。
     *
     * 否则：看 A 歌的评论 → 点 B 歌 → 在 B 的响应回来之前，
     * 评论页显示的还是 A 的评论（"短暂显示上一首歌的评论"）。
     * 写贴吧时踩过同一个坑，这里按同样的方式处理：先清，再请求。
     * 清空时同时把总数归零，标题栏的 "(N)" 也不会残留。
     */
    if (m_comments->size() == 0 && m_commentTotal == 0 && !m_commentsAutoOpen)
        return;

    m_comments->clear();
    m_commentTotal = 0;
    ++m_commentsVersion;
    emit commentsChanged();
}

void MusicController::requestCommentsForCurrent()
{
    m_commentsAutoOpen = true;
    loadCommentsForCurrent();
}

void MusicController::loadCommentsForIndex(int index)
{
    if (index < 0 || index >= m_songs->size()) {
        setError(NmTr("E697A0E69588E79A84E4B88BE6A087"));   // 无效的下标
        return;
    }

    const qint64 songId = m_songs->value(index).toMap()
                              .value(QLatin1String("id")).toString().toLongLong();
    if (songId <= 0) {
        setError(NmTr("E697A0E69588E79A84E4B88BE6A087"));
        return;
    }

    m_commentsTitle = NmTr("E6AD8CE69BB2E8AF84E8AEBA");   // 歌曲评论

    clearComments();
    setLoading(true);
    m_api->fetchComments(songId, 30, 0);
}

void MusicController::playIndex(int index)
{
    /*
     * ★ 下标一律以【显示中的模型 m_songs】为准：
     *   播放列表页可以在当前列表里做关键词过滤（setListFilter），
     *   过滤后 m_songs 和 m_songMaps（完整数据）的下标就不一致了。
     *   而列表点击传进来的 indexPath 是显示列表的下标，所以这里必须
     *   用 m_songs 取，否则过滤后会播错歌。
     */
    if (index < 0 || index >= m_songs->size()) {
        setError(NmTr("E697A0E69588E79A84E4B88BE6A087"));
        return;
    }

    const QVariantMap song = m_songs->value(index).toMap();

    // 立刻更新「正在播放」的显示（地址稍后异步到达）
    m_currentIndex = index;
    m_currentTrack = song;
    // ★ 换歌：封面回到"先占位、再切真图"的第一步（见 coverImagePath 的说明）
    m_coverReady = false;
    /*
     * ★ 起播后"推"封面几次（见 nudgeCover 的说明）：置 0 让封面先回到真图，
     *   随后几次推挤会交替引入零宽空格，把 SceneCover 的重绘带起来。
     *   封面图下载/复制需要时间，所以第一次推延后一点。
     */
    m_coverNudge = 0;
    QTimer::singleShot(600,  this, SLOT(nudgeCover()));
    QTimer::singleShot(1400, this, SLOT(nudgeCover()));
    QTimer::singleShot(2400, this, SLOT(nudgeCover()));
    QTimer::singleShot(3600, this, SLOT(nudgeCover()));

    /*
     * ★ 起播就【预取】640px 大图（bigImageCache）。
     *   封面（AppCover）和正在播放页用的都是这份缓存；不预取的话，
     *   第一次退到后台看封面时这张图还没下完，表现就是
     *   "要切一次歌封面才显示"。这里排队下载，切歌前就能拿到。
     *   pathFor() 内部：已缓存直接返回路径，没缓存就排队下载并返回空串。
     */
    if (m_bigImageCache) {
        const QString art = song.value(QLatin1String("artUrl")).toString();
        if (!art.isEmpty())
            m_bigImageCache->pathFor(art);
    }

    /*
     * 取这首的歌词：主界面迷你条（歌词开关打开时）要显示当前这句。
     * 换歌先把上一首的歌词清掉，避免短暂显示上一条。
     */
    if (m_lyricsEnabled) {
        const qint64 sid = song.value(QLatin1String("id")).toString().toLongLong();
        if (sid > 0) {
            m_lyricRaw.clear();
            m_transRaw.clear();
            m_lyricTimes.clear();
            m_lyricTexts.clear();
            m_lyricTrans.clear();
            if (!m_currentLyricLine.isEmpty()) {
                m_currentLyricLine.clear();
                emit currentLyricLineChanged();
                emit coverInfoChanged();
            }
            // ★ 翻译也要跟着清，否则换歌瞬间会显示上一首的翻译
            if (!m_currentLyricTrans.isEmpty()) {
                m_currentLyricTrans.clear();
                emit currentLyricTransChanged();
                emit coverInfoChanged();
            }
            m_api->fetchLyric(sid);
        }
    }

    // ★ 用户真的播了 → 当前列表被接管为队列（restoreList 据此不再还原）
    m_queueAdopted = true;

    ++m_currentTrackVersion;
    resetPlayer();
    emit currentTrackChanged();
    emit coverInfoChanged();
    /*
     * ★ 再补发一次（800ms 后）。
     *   真机反馈：起播后到歌词到达之间，多任务视图封面是一块黑，歌词一上来
     *   就恢复了 —— 说明只是"那一下"没重绘。补一次通知等价于人为制造一次
     *   内容变化，把这段窗口填上。
     */
    QTimer::singleShot(800, this, SLOT(notifyCoverAgain()));

    // 记一条本地播放历史（资料库的「最近播放」用）
    recordRecentPlayed(song);

    /*
     * 去重：同一首歌正在取地址时就别再发一次。
     * 真机日志里出现过两条一模一样的 player/url 请求（连点/双击造成），
     * 除了浪费一次请求，还会让播放器连续 setSource，触发
     * "Unable to set track parameters" 那一串告警。
     */
    const qint64 songId = song.value(QLatin1String("id")).toString().toLongLong();
    if (songId == m_pendingSongUrlId)
        return;
    m_pendingSongUrlId = songId;

    setLoading(true);
    m_api->fetchSongUrl(songId, m_session->bitrate());
}

bool MusicController::playNextSong(const QVariantMap &song)
{
    const QString id = song.value(QLatin1String("id")).toString();
    if (id.isEmpty())
        return false;

    /*
     * ★★「有没有正在播的歌」必须看 m_currentTrack，【绝不能看 m_currentIndex】！
     *
     *   clearSongs()（切歌单 / 重新搜索）会把 m_currentIndex 重置成 -1，
     *   但当前曲目是【继续播放】的 —— 那是故意的，见 clearSongs 的注释
     *   （"清空列表不等于停止播放"）。
     *
     *   踩过的坑：原来这里判 m_currentIndex < 0 就当成"没在播"，于是
     *   用户在【歌单页】长按「下一首播放」会直接切歌 —— 因为打开歌单时
     *   clearSongs() 已经把 m_currentIndex 变成 -1 了。
     */
    const bool hasTrack = m_currentTrackVersion > 0
                          && !m_currentTrack.value(QLatin1String("id")).toString().isEmpty();

    if (!hasTrack) {
        /*
         * 真的没有在播的曲目，「排到下一首」就没有意义了，退化成【直接播这一首】。
         *
         * ★ 这里千万不能用 setPlaylistFrom([song]) 顶掉当前列表：
         *   用户很可能正看着一个歌单（m_songs 非空、只是没在播），
         *   那样列表会瞬间只剩一首。所以：
         *     队列里已经有它 → 播那一个
         *     队列里没有它   → 插到【队首】再播（不改动列表其余内容）
         */
        int at = -1;
        for (int i = 0; i < m_songs->size(); ++i) {
            if (m_songs->value(i).toMap().value(QLatin1String("id")).toString() == id) {
                at = i;
                break;
            }
        }
        if (at < 0) {
            if (m_songs->size() == 0) {
                QVariantList one;
                one.append(song);
                setPlaylistFrom(one);
            } else {
                m_songs->insert(0, song);
                m_songMaps.insert(0, song);
            }
            at = 0;
        }
        playIndex(at);
        return true;
    }

    /*
     * 有歌在播。本项目的「播放队列」就是 m_songs —— next() 取的是
     * (m_currentIndex + 1) % m_songs->size()，所以「下一首播放」=
     * 把这首歌插到 m_songs 的 currentIndex 后面。
     *
     * ★ m_currentIndex 可能已经失效（切过列表 → -1，或还停在旧列表的下标上）：
     *   这时插到【队首】。next() 在 currentIndex 为 -1 时正好取下标 0，
     *   所以队首就是"下一首"，逻辑自洽。
     */
    const bool indexOk = m_currentIndex >= 0 && m_currentIndex < m_songs->size();
    const int at = indexOk ? m_currentIndex + 1 : 0;

    if (at < m_songs->size()
        && m_songs->value(at).toMap().value(QLatin1String("id")).toString() == id) {
        // 已经是下一首了，不用重复插（插了 next/prev 会连着放两遍同一首）
        return false;
    }

    /*
     * ★ 完整列表 m_songMaps 也要同步插一份：
     *   隐藏 VIP 歌 / setListFilter 过滤时，m_songs 是拿它重建的，
     *   不同步的话刚排的下一首一过滤就没了。
     *   下标有效时按【当前曲目的 id】定位再插到它后面 —— 过滤状态下
     *   m_songs 和 m_songMaps 的下标本来就不一致，不能直接按下标对齐。
     */
    int mapAt = -1;
    if (indexOk) {
        const QString curId = m_songs->value(m_currentIndex).toMap()
                                  .value(QLatin1String("id")).toString();
        for (int i = 0; i < m_songMaps.size(); ++i) {
            if (m_songMaps.at(i).toMap().value(QLatin1String("id")).toString() == curId) {
                mapAt = i + 1;
                break;
            }
        }
    }
    if (mapAt < 0 || mapAt > m_songMaps.size())
        mapAt = (at > m_songMaps.size()) ? m_songMaps.size() : at;

    m_songs->insert(at, song);
    m_songMaps.insert(mapAt, song);

    return true;
}

void MusicController::next()
{
    if (m_songs->size() == 0)
        return;

    // 随机播放：随机挑一首（尽量避开当前这首，免得连着放两遍同一首）
    if (m_shuffle && m_songs->size() > 1) {
        int r = m_currentIndex;
        while (r == m_currentIndex)
            r = qrand() % m_songs->size();
        playIndex(r);
        return;
    }

    const int index = (m_currentIndex + 1) % m_songs->size();
    playIndex(index);
}

void MusicController::prev()
{
    if (m_songs->size() == 0)
        return;
    const int index = (m_currentIndex - 1 + m_songs->size()) % m_songs->size();
    playIndex(index);
}

void MusicController::clearError()
{
    if (m_errorMessage.isEmpty())
        return;
    m_errorMessage.clear();
    m_errorDetail.clear();
    emit errorMessageChanged();
    emit errorDetailChanged();
}

void MusicController::notifyError(const QString &text)
{
    setError(text);

    /*
     * ★★ 关键：光 setError() 是【没人看的】—— 整个界面里没有任何地方绑定
     *   music.errorMessage，所以这些"提示"以前等同于石沉大海，
     *   用户侧表现就是"点了没反应"（真机反馈：点同一首歌该出提示却什么都没有）。
     *   这里必须同时弹一个系统 Toast。
     */
    showToast(text);
}

void MusicController::showToast(const QString &body)
{
    if (body.isEmpty() || !m_toast)
        return;

    /*
     * ★ 用常驻的 m_toast，**绝不能**在这里定义局部 SystemToast：
     *   局部对象在函数返回时就析构，提示根本来不及显示（BBTieba 的老坑）。
     */
    m_toast->setBody(body);
    m_toast->show();
}

void MusicController::trace(const QString &text)
{
    qWarning("[QML] %s", qPrintable(text));
}

void MusicController::setDebugEnabled(bool enabled)
{
    m_http->setDebugEnabled(enabled);
}

bool MusicController::debugEnabled() const
{
    return m_http->isDebugEnabled();
}

QString MusicController::debugLog() const
{
    return m_http->debugLog().join(QLatin1String("\n"));
}

void MusicController::clearDebugLog()
{
    m_http->clearDebugLog();
}

void MusicController::setConsoleLogEnabled(bool enabled)
{
    m_http->setConsoleLogEnabled(enabled);
}

bool MusicController::consoleLogEnabled() const
{
    return m_http->isConsoleLogEnabled();
}

bool MusicController::lyricsEnabled() const
{
    return m_lyricsEnabled;
}

void MusicController::setLyricsEnabled(bool on)
{
    if (m_lyricsEnabled == on)
        return;
    m_lyricsEnabled = on;

    QSettings settings;
    settings.setValue(QLatin1String("nm/lyrics"), m_lyricsEnabled);

    emit lyricsEnabledChanged();
    emit coverInfoChanged();

    /*
     * ★ 关掉「主界面显示歌词」时，翻译开关要【先跟着关掉】，QML 那边再把它
     *   置灰（SettingsPage 里 enabled 绑 music.lyricsEnabled）。
     *   翻译单独开着没有任何意义 —— 它只在歌词模式下才可能显示出来。
     *   顺序很关键：先关值、后禁用，界面上才不会出现"已禁用但仍亮着"的状态。
     */
    if (!m_lyricsEnabled)
        setLyricsTransEnabled(false);
}

QString MusicController::currentLyricLine() const
{
    return m_currentLyricLine;
}

bool MusicController::lyricsTransEnabled() const
{
    return m_lyricsTransEnabled;
}

void MusicController::setLyricsTransEnabled(bool on)
{
    if (m_lyricsTransEnabled == on)
        return;
    m_lyricsTransEnabled = on;

    QSettings settings;
    settings.setValue(QLatin1String("nm/lyricsTrans"), m_lyricsTransEnabled);

    emit lyricsTransEnabledChanged();
    emit coverInfoChanged();
}

QString MusicController::currentLyricTrans() const
{
    return m_currentLyricTrans;
}

int MusicController::songCount() const
{
    return m_songs->size();
}

int MusicController::songTotal() const
{
    // 和 songCount() 同值，只是多了 NOTIFY（见头文件里的说明）
    return m_songs->size();
}

int MusicController::browseCount() const
{
    return m_browseSongs->size();
}

int MusicController::browseTotal() const
{
    // 和 browseCount() 同值，只是多了 NOTIFY（见头文件里的说明）
    return m_browseSongs->size();
}

void MusicController::clearSongs()
{
    /*
     * 清空当前曲目列表（切换歌单 / 重新搜索时调用）。
     * 不清的话，新数据到达前列表里还是上一次的内容 —— 表现就是
     * "点了下一页，加载中还短暂显示上一页"（贴吧踩过同一个坑）。
     */
    m_songs->clear();
    m_songMaps.clear();
    m_albums->clear();
    m_currentIndex = -1;
    /*
     * ★ 不要清 m_currentTrack，也【不要动 m_currentTrackVersion】：
     *   清空列表（切歌单 / 搜索）不等于停止播放。
     *   之前这里 ++version 会把版本号顶到 1，而 QML 里"未播放"时
     *   currentTrack 是空 map（空对象在 JS 里是【真值】），于是 minibar
     *   判定 hasTrack=1 却 name=undefined，内容全空。
     *   真机稳定复现：未播放时点歌单再返回，迷你条/正在播放就全空。
     */
    ++m_albumsVersion;
    emit songsChanged();
    emit albumsChanged();
    emit currentTrackChanged();
    emit coverInfoChanged();
    /*
     * ★ 再补发一次（800ms 后）。
     *   真机反馈：起播后到歌词到达之间，多任务视图封面是一块黑，歌词一上来
     *   就恢复了 —— 说明只是"那一下"没重绘。补一次通知等价于人为制造一次
     *   内容变化，把这段窗口填上。
     */
    QTimer::singleShot(800, this, SLOT(notifyCoverAgain()));
}

void MusicController::setListFilter(const QString &text)
{
    const QString key = text.trimmed().toLower();

    /*
     * ★ 过滤目标跟着模式走：浏览模式过滤【浏览列表】，
     *   否则过滤播放队列。别把队列当过滤目标（歌单内搜索不该动队列）。
     */
    bb::cascades::ArrayDataModel *model = songsTarget();
    const QVariantList &maps = songsTargetMaps();

    model->clear();
    for (int i = 0; i < maps.size(); ++i) {
        const QVariantMap map = maps.at(i).toMap();
        if (songVisible(map)
                && (key.isEmpty()
                    || map.value(QLatin1String("name")).toString().toLower().contains(key)
                    || map.value(QLatin1String("artistsText")).toString().toLower().contains(key)
                    || map.value(QLatin1String("albumName")).toString().toLower().contains(key))) {
            model->append(map);
        }
    }

    // 注意：过滤后列表下标和完整 maps 不再一致，
    // 所以播放时传的 index 指的是**当前显示列表**的下标。
    if (m_browseMode)
        emit browseSongsChanged();
    else
        emit songsChanged();
}

QString MusicController::listTitle() const
{
    const QString name = m_playlistInfo.value(QLatin1String("name")).toString();
    if (!name.isEmpty())
        return name;
    return m_lastQuery;
}

int MusicController::commentCount() const
{
    return m_comments->size();
}

int MusicController::bitrate() const
{
    return m_session->bitrate();
}

void MusicController::setBitrate(int br)
{
    m_session->setBitrate(br);
}

QString MusicController::theme() const
{
    return m_theme;
}

void MusicController::setTheme(const QString &theme)
{
    // 只认这两个值；其它（含空串）一律当"跟随系统"
    if (theme == QLatin1String("Dark") || theme == QLatin1String("Bright"))
        m_theme = theme;
    else
        m_theme.clear();

    QSettings settings;
    settings.setValue(QLatin1String("nm/theme"), m_theme);

    applyThemeStyle();
    emit themeChanged();
}

bool MusicController::hideVip() const
{
    return m_hideVip;
}

void MusicController::setHideVip(bool hide)
{
    if (m_hideVip == hide)
        return;
    m_hideVip = hide;

    QSettings settings;
    settings.setValue(QLatin1String("nm/hideVip"), m_hideVip);

    emit hideVipChanged();

    // maps 始终完整，直接重填三个显示模型即可立即生效，无需重新联网
    refillSongModels();
}

bool MusicController::songVisible(const QVariantMap &map) const
{
    return !m_hideVip || !map.value(QLatin1String("isVip")).toBool();
}

void MusicController::refillSongModels()
{
    const QVariantList *mapLists[4] = {
        &m_songMaps, &m_recommendSongMaps, &m_artistSongMaps, &m_browseSongMaps
    };
    bb::cascades::ArrayDataModel *models[4] = {
        m_songs, m_recommendSongs, m_artistSongs, m_browseSongs
    };

    for (int k = 0; k < 4; ++k) {
        models[k]->clear();
        const QVariantList &maps = *mapLists[k];
        for (int i = 0; i < maps.size(); ++i) {
            const QVariantMap map = maps.at(i).toMap();
            if (songVisible(map))
                models[k]->append(map);
        }
    }

    emit songsChanged();
    emit recommendSongsChanged();
    emit artistSongsChanged();

    // 「全部歌曲」也按开关重建（排序 + 分组）
    rebuildAllSongs();
}

void MusicController::requestOpenPlaylist(const QString &id)
{
    m_openRequestKind = QLatin1String("playlist");
    m_openRequestArg = id;
    // ★ 不在这里直接发信号，推迟 200ms 再通知 QML 推页（见 scheduleOpenRequest）
    scheduleOpenRequest();
}

void MusicController::requestOpenNowPlaying()
{
    m_openRequestKind = QLatin1String("nowplaying");
    m_openRequestArg.clear();
    // ★ 同样推迟 200ms 再通知 QML 推页（见 scheduleOpenRequest）
    scheduleOpenRequest();
}

void MusicController::requestOpenProperties()
{
    // 不带歌时默认看【当前播放】这一首
    requestOpenPropertiesFor(m_currentTrack);
}

void MusicController::requestOpenPropertiesFor(const QVariantMap &song)
{
    const QString id = song.value(QLatin1String("id")).toString();
    // 诊断：确认从列表菜单传进来的到底是什么（名字为空 = 详情页必然显示"没有歌曲"）
    qWarning("MusicController: requestOpenPropertiesFor id=%s name=%s keys=%d",
             qPrintable(id),
             qPrintable(song.value(QLatin1String("name")).toString()),
             song.size());
    if (id.isEmpty())
        return;

    m_propertiesTrack = song;
    m_propertiesLyric.clear();
    m_propertiesLyricFetching = false;

    /*
     * 歌词：目标就是"正在播放这一首"时直接用手上那份，省一次请求；
     * 是列表里别的歌则单独拉一份。
     * ★ 单独拉时只写 m_propertiesLyric，【不碰】m_lyricRaw / m_lyricTimes ——
     *   那些是主界面迷你条在用的，顶掉就会串词（分流见 onLyricFinished）。
     */
    const QString curId = m_currentTrack.value(QLatin1String("id")).toString();
    if (!curId.isEmpty() && curId == id) {
        m_propertiesLyric = m_lyricRaw;
    } else {
        const qint64 sid = id.toLongLong();
        if (sid > 0) {
            m_propertiesLyricFetching = true;
            m_api->fetchLyric(sid);
        }
    }

    ++m_propertiesVersion;
    emit propertiesChanged();

    m_openRequestKind = QLatin1String("properties");
    m_openRequestArg.clear();
    scheduleOpenRequest();
}

void MusicController::clearQueue()
{
    /*
     * ★ 只清队列，不停播：正在放的这首继续放完（和 clearSongs() 同一套约定，
     *   见那里的注释"清空列表不等于停止播放"）。
     */
    m_imageCache->cancelQueued();
    m_songs->clear();
    m_songMaps.clear();
    m_currentIndex = -1;
    emit songsChanged();
}

void MusicController::removeSongAt(int index)
{
    if (index < 0 || index >= m_songs->size())
        return;

    const QString id = m_songs->value(index).toMap()
                           .value(QLatin1String("id")).toString();

    m_songs->removeAt(index);

    /*
     * 完整列表 m_songMaps 里也要删掉同一首。
     * ★ 按 id 定位，不能按下标：隐藏 VIP / setListFilter 过滤时两边下标不对齐。
     */
    for (int i = 0; i < m_songMaps.size(); ++i) {
        if (m_songMaps.at(i).toMap().value(QLatin1String("id")).toString() == id) {
            m_songMaps.removeAt(i);
            break;
        }
    }

    /*
     * 当前播放下标跟着挪：
     *   删的是它前面（或就是它）→ 下标 -1。删掉的正是正在播那首时，
     *   next() 就会接着放它后面那首（原来 index 位置现在被下一首顶上了）。
     */
    if (index <= m_currentIndex)
        --m_currentIndex;
    if (m_currentIndex < -1)
        m_currentIndex = -1;

    emit songsChanged();
}

void MusicController::requestOpenQueue()
{
    m_openRequestKind = QLatin1String("queue");
    m_openRequestArg.clear();
    // ★ 不在这里直接发信号，推迟 200ms 再通知 QML 推页（见 scheduleOpenRequest）
    scheduleOpenRequest();
}

void MusicController::copyToClipboard(const QString &text)
{
    if (text.isEmpty())
        return;

    bb::system::Clipboard clipboard;
    clipboard.clear();
    clipboard.insert(QLatin1String("text/plain"), text.toUtf8());
}

QString MusicController::clipboardText() const
{
    bb::system::Clipboard clipboard;

    /*
     * 剪贴板里同一份内容常同时存着多种类型（text/html 等），
     * 这里只要纯文本那份 —— 就是 copyToClipboard 写进去的那个 key。
     */
    const QByteArray data = clipboard.value(QLatin1String("text/plain"));
    if (data.isEmpty())
        return QString();

    return QString::fromUtf8(data);
}

void MusicController::scheduleOpenRequest()
{
    /*
     * 推迟 200ms 再通知 QML 推页 —— 等长按菜单 / 应用菜单的关闭动画走完。
     *
     * 立刻通知的话，那份 "peek 关闭" 动画会被 NavigationPane 当成向左滑返回，
     * 刚推上去的页立刻被判 isSuggestedStackOk 弹回来（真机日志）：
     *     pushPage: pushed, top=userDetailPage
     *     NavigationPane::isSuggestedStackOk: suggested: "27,458,971"
     *     NavigationPaneOnBackTransitionDone: popped page: 1208
     * 用户侧的表现就是"同一下要点两次才进得去"。
     *
     * ★ 连续点两次只保留最后一次：start() 会重置计时。
     */
    m_openRequestTimer->start();
}

void MusicController::onOpenRequestTimeout()
{
    // 到点了：此时菜单动画已经结束，推页不会再被误判成返回手势
    ++m_openRequestVersion;
    emit openRequestChanged();
}

void MusicController::requestOpenUser(const QString &uid,
                                      const QString &nickName,
                                      const QString &avatarUrl)
{
    if (uid.isEmpty()) {
        qWarning("MusicController: requestOpenUser ignored (empty uid)");
        return;
    }

    // 诊断：uid 有没有从评论页传到 C++（真机日志直接看得到）
    qWarning("MusicController: requestOpenUser uid=%s name=%s",
             qPrintable(uid), qPrintable(nickName));

    /*
     * ★ 昵称/头像评论数据里就有，先塞进 viewedUser ——
     *   页面一推出来就能显示，不用等 /user/playlist 回来，
     *   否则会先闪一下"空头像 + 未命名"。
     */
    QVariantMap user;
    user.insert(QLatin1String("userId"), uid);
    user.insert(QLatin1String("nickname"), nickName);
    user.insert(QLatin1String("avatarUrl"), avatarUrl);
    m_viewedUser = user;
    emit viewedUserChanged();

    m_openRequestKind = QLatin1String("user");
    m_openRequestArg = uid;
    // ★ 不在这里直接发信号，推迟 200ms 再通知 QML 推页（见 scheduleOpenRequest）
    scheduleOpenRequest();
}

void MusicController::loadViewedPlaylists(const QString &uid)
{
    const qint64 id = uid.toLongLong();
    if (id <= 0)
        return;

    // 去重：正在拉就不重复发（和 loadMyPlaylists 同一个标记）
    if (m_playlistsFetching)
        return;
    m_playlistsFetching = true;

    // 结果进 viewedPlaylists，见 onUserPlaylistsFinished 里的分流
    m_viewedUserMode = true;
    setLoading(true);
    m_api->fetchUserPlaylists(id);

    /*
     * 顺手把他的资料也拉回来（等级 / 听歌数 / 签名 / 粉丝数）——
     * 这样别人的资料页和自己的长得一样，而不是只有头像和名字。
     * ★ 注意：/api/artist/sublist 【不认 uid】，别人的"关注歌手"拿不到，
     *   所以那一栏对别人是空的（见 UserDetailParse 的说明）。
     */
    m_api->fetchUserDetail(id);
}

void MusicController::onUserDetailFinished(
        int requestId, const nm::NmParsers::UserDetailParse &result)
{
    Q_UNUSED(requestId);

    // 拿不到不影响歌单列表，静默处理
    if (!result.ok)
        return;

    /*
     * 补全 m_viewedUser。★ 只动它，绝不碰 m_nickName / m_userLevel 那些
     * "我的"字段（否则用户 Tab 会被别人的数据污染）。
     */
    if (!result.nickName.isEmpty())
        m_viewedUser.insert(QLatin1String("nickname"), result.nickName);
    if (!result.avatarUrl.isEmpty())
        m_viewedUser.insert(QLatin1String("avatarUrl"), result.avatarUrl);
    m_viewedUser.insert(QLatin1String("signature"), result.signature);
    m_viewedUser.insert(QLatin1String("level"), result.level);
    m_viewedUser.insert(QLatin1String("listenSongs"), result.listenSongs);
    m_viewedUser.insert(QLatin1String("follows"), result.follows);
    m_viewedUser.insert(QLatin1String("followeds"), result.followeds);
    if (result.artistId > 0) {
        // 对方是音乐人：记下来（以后可以加个"查看他的艺人页"入口）
        m_viewedUser.insert(QLatin1String("artistId"),
                            QString::number(result.artistId));
        m_viewedUser.insert(QLatin1String("artistName"), result.artistName);
    }

    emit viewedUserChanged();
}

void MusicController::requestOpenSearch(const QString &name)
{
    m_openRequestKind = QLatin1String("search");
    m_openRequestArg = name;
    // ★ 不在这里直接发信号，推迟 200ms 再通知 QML 推页（见 scheduleOpenRequest）
    scheduleOpenRequest();
}

void MusicController::requestOpenArtist(const QString &name)
{
    m_openRequestKind = QLatin1String("artist");
    m_openRequestArg = name;
    // ★ 不在这里直接发信号，推迟 200ms 再通知 QML 推页（见 scheduleOpenRequest）
    scheduleOpenRequest();
}

QVariantMap MusicController::artistInfo(const QString &name) const
{
    for (int i = 0; i < m_artists->size(); ++i) {
        const QVariantMap map = m_artists->value(i).toMap();
        if (map.value(QLatin1String("name")).toString() == name)
            return map;
    }
    return QVariantMap();
}

void MusicController::clearOpenRequest()
{
    // 不清版本号：main.qml 靠 kind 是否为空判断，清版本号反而会再触发一次
    m_openRequestKind.clear();
    m_openRequestArg.clear();
}

void MusicController::attachPlayer(bb::multimedia::MediaPlayer *player)
{
    m_mediaPlayer = player;
    // 进度用播放器的 positionChanged 信号驱动（照 ModPlayer），不轮询：
    // 模拟器上轮询 position() 会几秒才跳一次、甚至卡住不动。
    if (m_mediaPlayer) {
        connect(m_mediaPlayer, SIGNAL(positionChanged(unsigned int)),
                this, SLOT(onPlayerPositionChanged(unsigned int)));
    }
}

void MusicController::seekTo(int msec)
{
    if (!m_mediaPlayer)
        return;
    // 照 ModPlayer：不可 seek 的源直接跳过，省得抛 InvalidState
    if (!m_mediaPlayer->isSeekable()) {
        qWarning("[SEEK] 当前曲目不可 seek，忽略 seekTo(%d)", msec);
        return;
    }
    const int target = msec < 0 ? 0 : msec;
    const int err = static_cast<int>(
        m_mediaPlayer->seekTime(static_cast<unsigned int>(target)));
    qWarning("[SEEK] seekTo(%d) err=%d", msec, err);
    /*
     * ★ 立刻把进度更新为跳转目标并通知 QML。
     *   否则下一拍 position() 还没反映新位置时，进度条的 value 绑定会把
     *   滑块拽回旧值 —— 表现为「松手后回弹一下」。
     */
    if (target != m_playerPosition) {
        m_playerPosition = target;
        emit playerPositionChanged();
    }
}

int MusicController::playerPosition() const
{
    return m_playerPosition;
}

void MusicController::onPlayerPositionChanged(unsigned int position)
{
    if (m_seeking)   // 拖动进度条期间不更新，防回弹
        return;
    const int p = static_cast<int>(position);
    if (p == m_playerPosition)
        return;
    m_playerPosition = p;
    emit playerPositionChanged();
    // 同步歌词（主界面迷你条用）
    updateLyricForPosition(m_playerPosition);
}

void MusicController::onPositionTick()
{
    if (m_seeking || !m_mediaPlayer)   // 拖动期间不更新
        return;
    const int p = static_cast<int>(m_mediaPlayer->position());
    if (p < 0 || p == m_playerPosition)
        return;
    m_playerPosition = p;
    emit playerPositionChanged();
    // 同步歌词（模拟器靠这条轮询兜底）
    updateLyricForPosition(m_playerPosition);
}

void MusicController::setSeeking(bool seeking)
{
    m_seeking = seeking;
}

int MusicController::repeatMode() const
{
    return m_repeatMode;
}

void MusicController::setRepeatMode(int m)
{
    // 循环只有两个状态：1=列表循环 2=单曲循环（随机是独立开关，见 m_shuffle）
    if (m < 1 || m > 2)
        m = 1;
    if (m_repeatMode == m)
        return;
    m_repeatMode = m;
    emit repeatModeChanged();
}

void MusicController::toggleRepeatMode()
{
    // 列表循环 ⇄ 单曲循环
    setRepeatMode((m_repeatMode == 2) ? 1 : 2);
}

bool MusicController::shuffle() const
{
    return m_shuffle;
}

void MusicController::setShuffle(bool on)
{
    if (m_shuffle == on)
        return;
    m_shuffle = on;
    emit shuffleChanged();
}

void MusicController::toggleShuffle()
{
    /*
     * ★ 只切开关、不动当前曲目：开了随机也要把正在放的这首放完，
     *   下一首才走随机挑歌（随机逻辑在 next() 里）。
     */
    setShuffle(!m_shuffle);
}

void MusicController::applySavedTheme()
{
    QSettings settings;
    m_theme = settings.value(QLatin1String("nm/theme")).toString();
    applyThemeStyle();
}

void MusicController::applyThemeStyle()
{
    Application *app = Application::instance();
    if (!app || !app->themeSupport())
        return;

    /*
     * ""（跟随系统）时不动：bar-descriptor 的 CASCADES_THEME=default 已经把
     * 视觉风格设好了，强行再设一次反而会覆盖"跟随系统"。
     * 注意 setVisualStyle 只改视觉风格、不动主色（主色仍是 descriptor 里的红）。
     */
    if (m_theme == QLatin1String("Dark"))
        app->themeSupport()->setVisualStyle(VisualStyle::Dark);
    else if (m_theme == QLatin1String("Bright"))
        app->themeSupport()->setVisualStyle(VisualStyle::Bright);
}

void MusicController::loadRecentPlayed()
{
    QSettings settings;
    const QVariantList list = settings.value(QLatin1String("nm/recent")).toList();

    // 标题（页头的名字 / 首数）先铺好，和歌单页共用同一套 playlistInfo
    m_lastQuery.clear();
    m_playlistInfo.clear();
    m_playlistInfo.insert(QLatin1String("id"), QString());
    m_playlistInfo.insert(QLatin1String("name"),
                          NmTr("E69C80E8BF91E692ADE694BE"));   // 最近播放
    m_playlistInfo.insert(QLatin1String("trackCount"), list.size());
    ++m_playlistInfoVersion;
    emit playlistInfoChanged();

    /*
     * ★★ 必须填【浏览模型】——「最近播放」复用的是播放列表页，它绑的是
     *   music.browseSongs。原来这里填的是 m_songs（全局播放队列），
     *   表现就是"最近播放显示的内容不对"（实际是上一次打开的歌单/专辑），
     *   和 loadLibraryArtist 当初是同一个坑。
     */
    m_browseMode = true;
    m_artistMode = false;
    m_recommendMode = false;

    m_imageCache->cancelQueued();
    m_browseSongs->clear();
    m_browseSongMaps.clear();
    for (int i = 0; i < list.size(); ++i) {
        QVariantMap map = list.at(i).toMap();
        map.insert(QLatin1String("localArtPath"),
                   m_imageCache->pathFor(
                       map.value(QLatin1String("artUrl")).toString()));
        map.insert(QLatin1String("type"), QLatin1String("song"));
        m_browseSongMaps.append(map);
        if (songVisible(map))
            m_browseSongs->append(map);
    }
    ++m_browseSongsVersion;
    emit browseSongsChanged();

    if (list.isEmpty())
        setError(NmTr("E8BF98E6B2A1E69C89E692ADE694BEE8AEB0E5BD95")); // 还没有播放记录
}

void MusicController::recordRecentPlayed(const QVariantMap &song)
{
    const QString id = song.value(QLatin1String("id")).toString();
    if (id.isEmpty())
        return;

    QSettings settings;
    QVariantList list = settings.value(QLatin1String("nm/recent")).toList();

    // 同一首只留最新一次
    for (int i = list.size() - 1; i >= 0; --i) {
        if (list.at(i).toMap().value(QLatin1String("id")).toString() == id)
            list.removeAt(i);
    }

    // 存精简副本（本地封面路径下次再算）
    QVariantMap slim = song;
    slim.remove(QLatin1String("localArtPath"));
    list.prepend(slim);
    while (list.size() > 50)
        list.removeLast();

    settings.setValue(QLatin1String("nm/recent"), list);
}

QString MusicController::bannerPath() const
{
    /*
     * 个人主页背景：从【本地已缓存】的封面里随机挑一张。
     * ★ 不拿正在播放曲目的封面（用户要求背景与当前播放无关）。
     * ★ 选到一张后【固定下来】（m_bannerPath）：启动后不再换，避免"背景一直在动"。
     *   还没图时返回空（不缓存），QML 侧等图片下载完会再问一次，直到选到为止。
     */
    if (!m_bannerPath.isEmpty())
        return m_bannerPath;

    QStringList pool;
    bb::cascades::ArrayDataModel *models[3] = {
        m_albums, m_playlists, m_discoverPlaylists
    };
    for (int k = 0; k < 3; ++k) {
        for (int i = 0; i < models[k]->size(); ++i) {
            const QString p = models[k]->value(i).toMap()
                                  .value(QLatin1String("localCoverPath")).toString();
            if (!p.isEmpty())
                pool << p;
        }
    }

    if (pool.isEmpty())
        return QString();
    m_bannerPath = pool.at(qrand() % pool.size());
    return m_bannerPath;
}

/*
 * ★ 历史遗留说明（相关代码已删）：早期「全部歌曲」用的是平铺 ArrayDataModel，
 *   它的 itemType() 恒为空串（NDK 头文件写明），只能给 ListView 挂一个
 *   ListItemTypeMapper（NDK 的 listitemtypemapper.h）才能分出 header 项。
 *   现在 allSongs 已经是 GroupDataModel —— 分组头原生就是 "header" 类型，
 *   所以那套 mapper 不再需要，已连同 include 一起移除。
 */

// ---- 「全部歌曲」排序 / 字母分组用的文件内辅助 ----
static QString nmSongSectionKey(const QString &name)
{
    /*
     * ★ 数字/符号统一并进最后一个分组。
     *   GroupDataModel 的分组键既是"排序依据"又是"header 显示文本"，
     *   所以这里用一个高值哨兵（U+FFFF）保证它排到最后，
     *   QML 侧再把它显示成 "*"（见 main.qml 的 header 组件）。
     */
    if (name.isEmpty())
        return QString(QChar(0xFFFF));
    const QChar c = name.at(0).toUpper();
    const ushort u = c.unicode();
    // A-Z 各自成组
    if (u >= 'A' && u <= 'Z')
        return QString(c);
    // 汉字、假名等其它字母：保留自己的首字分组
    if (c.isLetter())
        return QString(c);
    // 数字 0-9 与符号 → 最后一个分组（显示为 "*"）
    return QString(QChar(0xFFFF));
}

static bool nmSongMapByName(const QVariantMap &a, const QVariantMap &b)
{
    // 先按分组键排（"*" 用 0xFFFF 顶到所有字母/汉字之后），
    // 同一组内再按名字不区分大小写排。
    QString ka = nmSongSectionKey(a.value(QLatin1String("name")).toString());
    QString kb = nmSongSectionKey(b.value(QLatin1String("name")).toString());
    if (ka == QLatin1String("*"))
        ka = QString(QChar(0xFFFF));
    if (kb == QLatin1String("*"))
        kb = QString(QChar(0xFFFF));
    if (ka != kb)
        return ka < kb;
    return a.value(QLatin1String("name")).toString()
        .compare(b.value(QLatin1String("name")).toString(), Qt::CaseInsensitive) < 0;
}

/*! 一个歌单是不是【收藏】来的（完整实现见下方 rebuildLibraryPlaylists 附近） */
static bool nmPlaylistSubscribed(const QVariantMap &row);
/*! 两份曲目列表是否内容一致（完整实现见下方 fillAllSongsModel 附近） */
static bool sameSongMaps(const QVariantList &a, const QVariantList &b);

void MusicController::loadAllSongs()
{
    // 先显示本地缓存（下次进来不用等网络）
    loadAllSongsCache();

    /*
     * ★★ 合并【我创建的所有歌单】，不再只拉「我喜欢的音乐」一个。
     *   以前只有"我喜欢的音乐"里的歌会进「全部歌曲」，用户另外自建的
     *   歌单一首都不进（真机反馈）。
     *
     *   收藏（订阅）来的歌单【不算】—— 那不是"我的"音乐。
     *   串行拉取（一次一个请求，避免一次打十几个），全部回来后再合并去重，
     *   见 appendAllSongsBatch / finishAllSongsFetch。
     */
    m_allSongsPendingIds.clear();

    // 「我喜欢的音乐」排最前（它是官方的"全部歌曲"基准）
    if (!m_favoritePlaylistId.isEmpty())
        m_allSongsPendingIds.append(m_favoritePlaylistId);

    for (int i = 0; i < m_playlists->size(); ++i) {
        const QVariantMap row = m_playlists->value(i).toMap();
        if (nmPlaylistSubscribed(row))
            continue;                               // 收藏的歌单不要
        const QString id = row.value(QLatin1String("id")).toString();
        if (id.isEmpty() || m_allSongsPendingIds.contains(id))
            continue;
        m_allSongsPendingIds.append(id);
    }

    if (m_allSongsPendingIds.isEmpty()) {
        setError(QString::fromUtf8("还没有歌单，请先登录并刷新我的歌单"));
        return;
    }

    m_allSongsMode = true;
    m_allSongsFetching = true;
    m_allSongsFetch.clear();
    m_allSongsFetchIds.clear();
    m_imageCache->cancelQueued();
    setLoading(true);

    m_allSongsFetchId = m_allSongsPendingIds.takeFirst();
    m_api->fetchPlaylist(m_allSongsFetchId);
}

/*!
 * 把一批曲目累积进「全部歌曲」的合并结果。
 * ★ 跨歌单去重：同一首歌常常同时躺在「我喜欢的音乐」和别的自建歌单里。
 */
void MusicController::appendAllSongsBatch(const QList<nm::NmSong> &songs)
{
    foreach (const nm::NmSong &song, songs) {
        QVariantMap map = song.toVariantMap();
        const QString id = map.value(QLatin1String("id")).toString();
        if (id.isEmpty() || m_allSongsFetchIds.contains(id))
            continue;
        m_allSongsFetchIds.append(id);
        /*
         * ★「全部歌曲」页不显示封面（照官方 Music ui 的 All Songs）：
         *   localArtPath 留空，655 首就不会瞬间塞 655 个图片请求。
         */
        map.insert(QLatin1String("localArtPath"), QString());
        map.insert(QLatin1String("type"), QLatin1String("song"));
        m_allSongsFetch.append(map);
    }
}

/*! 所有歌单都拉完了：比对后重建模型 + 写缓存 */
void MusicController::finishAllSongsFetch()
{
    m_allSongsMode = false;
    m_allSongsFetching = false;

    /*
     * ★ QVariantList 不能直接套 QVariantMap 的比较函数排序 —— qSort 传进去的
     *   是 QVariant&，对不上形参的 const QVariantMap&（编译期直接报错）。
     *   先转成 QList<QVariantMap> 排好再转回来，和 rebuildAllSongs 一致。
     */
    QList<QVariantMap> sorted;
    for (int i = 0; i < m_allSongsFetch.size(); ++i)
        sorted.append(m_allSongsFetch.at(i).toMap());
    qSort(sorted.begin(), sorted.end(), nmSongMapByName);

    m_allSongsFetch.clear();
    for (int i = 0; i < sorted.size(); ++i)
        m_allSongsFetch.append(sorted.at(i));

    /*
     * ★ 内容没变就【不重建】：进「全部歌曲」会先用本地缓存铺一遍，
     *   网络回来再填一次；每次都 clear + 全量重插 + 排序，界面就会闪。
     */
    if (!sameSongMaps(m_allSongMaps, m_allSongsFetch)) {
        m_allSongMaps = m_allSongsFetch;
        rebuildAllSongs();
    }
    saveAllSongsCache();
    finishRequest(QLatin1String("allSongs"), true);
}

/*! 两份曲目列表是否【内容一致】（只比 id 序列，够用且稳） */
static bool sameSongMaps(const QVariantList &a, const QVariantList &b)
{
    if (a.size() != b.size())
        return false;
    for (int i = 0; i < a.size(); ++i) {
        if (a.at(i).toMap().value(QLatin1String("id")).toString()
            != b.at(i).toMap().value(QLatin1String("id")).toString())
            return false;
    }
    return true;
}

void MusicController::fillAllSongsModel(const QList<nm::NmSong> &songs)
{
    m_imageCache->cancelQueued();

    // 先装进临时列表，和手上的数据比一比再决定要不要重建
    QVariantList next;
    foreach (const nm::NmSong &song, songs) {
        QVariantMap map = song.toVariantMap();
        /*
         * ★ 官方音乐「全部歌曲」页是不显示封面的。这里就【不排队下载】封面：
         *   655 首会瞬间塞 655 个图片请求，全歌曲列表最耗时的就是这块。
         *   localArtPath 留空，页面用 showImage:false 直接不渲染图。
         */
        map.insert(QLatin1String("localArtPath"), QString());
        map.insert(QLatin1String("type"), QLatin1String("song"));
        next.append(map);
    }

    /*
     * ★ 内容没变就【不要重建】。
     *   进「全部歌曲」时会走两遍：缓存先重建一次，网络回来 fill 又重建一次。
     *   每次重建都是 GroupDataModel 的 clear + 655 首全量重插 + 排序，
     *   界面上表现就是"闪一下"（用户反馈：启动后第一次点进去会闪）。
     *   两份内容一致时直接跳过，既消掉闪烁也省一次全量排序。
     */
    if (sameSongMaps(m_allSongMaps, next)) {
        saveAllSongsCache();
        return;
    }

    m_allSongMaps = next;

    rebuildAllSongs();
    saveAllSongsCache();
}

void MusicController::playAllSongById(const QString &id)
{
    if (id.isEmpty())
        return;

    /*
     * ★ 模型是 GroupDataModel，ListItem 的 indexPath 是 [组, 项] 两层，
     *   没法再按下标播；改为按 id 定位（id 唯一）。
     *   播放列表用和 rebuildAllSongs() 同一套过滤 + 排序，保证 next/prev 顺序一致。
     */
    QList<QVariantMap> maps;
    for (int i = 0; i < m_allSongMaps.size(); ++i) {
        const QVariantMap m = m_allSongMaps.at(i).toMap();
        if (songVisible(m))
            maps.append(m);
    }
    qSort(maps.begin(), maps.end(), nmSongMapByName);

    QVariantList songs;
    int target = -1;
    foreach (const QVariantMap &m, maps) {
        if (target < 0 && m.value(QLatin1String("id")).toString() == id)
            target = songs.size();
        songs.append(m);
    }
    if (target < 0)
        return;
    setPlaylistFrom(songs);
    m_allSongsMode = false;
    playIndex(target);
}

void MusicController::setAllSongsFilter(const QString &text)
{
    const QString kw = text.trimmed();
    if (m_allSongsFilter == kw)
        return;
    m_allSongsFilter = kw;
    // 过滤只改显示，不发请求；VIP 开关变化时也会走这里同一套重建
    rebuildAllSongs();
}

void MusicController::playAllSongsShuffled()
{
    QList<QVariantMap> maps;
    for (int i = 0; i < m_allSongMaps.size(); ++i) {
        const QVariantMap m = m_allSongMaps.at(i).toMap();
        if (songVisible(m))
            maps.append(m);
    }
    if (maps.isEmpty())
        return;

    // Fisher–Yates 洗牌
    for (int i = maps.size() - 1; i > 0; --i) {
        const int j = qrand() % (i + 1);
        qSwap(maps[i], maps[j]);
    }

    QVariantList songs;
    foreach (const QVariantMap &m, maps)
        songs.append(m);

    // 洗牌后的列表当当前播放队列（next/prev 也按这个顺序走）
    setPlaylistFrom(songs);
    m_allSongsMode = false;
    playIndex(0);
}

void MusicController::rebuildAllSongs()
{
    m_allSongs->clear();

    // 页内搜索关键词（空 = 不过滤）。★ 和 VIP 隐藏开关是【叠加】的两层过滤
    const QString kw = m_allSongsFilter.trimmed();

    QList<QVariantMap> maps;
    for (int i = 0; i < m_allSongMaps.size(); ++i) {
        const QVariantMap m = m_allSongMaps.at(i).toMap();
        if (!songVisible(m))
            continue;
        if (!kw.isEmpty()
            && !m.value(QLatin1String("name")).toString().contains(kw, Qt::CaseInsensitive)
            && !m.value(QLatin1String("artistsText")).toString().contains(kw, Qt::CaseInsensitive)
            && !m.value(QLatin1String("albumName")).toString().contains(kw, Qt::CaseInsensitive))
            continue;
        maps.append(m);
    }

    qSort(maps.begin(), maps.end(), nmSongMapByName);

    /*
     * ★ 只把 sectionKey 写进每条曲目，分组由 GroupDataModel 自己完成 ——
     *   它会把每个分组的头部输出成 "header" 类型项，配
     *   ListHeaderMode::StickyOverlay 就是"置顶、被下一个顶掉"的效果。
     *   不再自己插 isHeader 项（那是平铺模型才需要的做法）。
     */
    foreach (const QVariantMap &m, maps) {
        QVariantMap item = m;
        item.insert(QLatin1String("sectionKey"),
                    nmSongSectionKey(m.value(QLatin1String("name")).toString()));
        m_allSongs->insert(item);
    }

    ++m_allSongsVersion;
    emit allSongsChanged();

    // 「艺术家」索引和它同源（都从 m_allSongMaps 来），顺手一起重建
    rebuildLibraryArtists();
}

void MusicController::rebuildLibraryArtists()
{
    m_libraryArtists->clear();

    /*
     * 按 artistsText 归类。★ 用 QMap 保序 + 单独一个 QList 记顺序，
     * 最后按艺人名排序（和 Apple Music 的艺术家页一样有稳定顺序）。
     */
    QMap<QString, QVariantMap> byArtist;
    QList<QString> order;

    for (int i = 0; i < m_allSongMaps.size(); ++i) {
        const QVariantMap m = m_allSongMaps.at(i).toMap();
        if (!songVisible(m))
            continue;

        const QString name = m.value(QLatin1String("artistsText")).toString().trimmed();
        if (name.isEmpty())
            continue;

        if (!byArtist.contains(name)) {
            QVariantMap a;
            a.insert(QLatin1String("name"), name);
            a.insert(QLatin1String("songCount"), 0);
            // 艺人没有头像，先拿他第一首歌的封面顶着（比占位图好看）
            a.insert(QLatin1String("artUrl"), m.value(QLatin1String("artUrl")));
            a.insert(QLatin1String("type"), QLatin1String("libraryArtist"));
            byArtist.insert(name, a);
            order.append(name);
        }

        QVariantMap a = byArtist.value(name);
        a.insert(QLatin1String("songCount"), a.value(QLatin1String("songCount")).toInt() + 1);
        byArtist.insert(name, a);
    }

    qSort(order.begin(), order.end());

    foreach (const QString &name, order) {
        QVariantMap a = byArtist.value(name);
        a.insert(QLatin1String("localCoverPath"),
                 m_imageCache->pathFor(a.value(QLatin1String("artUrl")).toString()));
        /*
         * ★ 分组键（和「全部歌曲」用同一个函数）：A-Z 各自成组、汉字各自成组、
         *   数字与符号并进 U+FFFF 那组（显示成 "*"）。GroupDataModel 会按它
         *   分组，并把每个组的头部输出成 "header" 类型项。
         */
        a.insert(QLatin1String("sectionKey"), nmSongSectionKey(name));
        m_libraryArtists->insert(a);
    }

    ++m_libraryArtistsVersion;
    emit libraryArtistsChanged();
}

int MusicController::libraryArtistCount() const
{
    return m_libraryArtists->size();
}

/*!
 * 一个歌单是不是【收藏】来的。
 *
 * ★ 不能直接用 QVariant::toBool()：
 *   这份数据会过一趟 QSettings（本地缓存），布尔到那儿就变成字符串了，
 *   而 Qt 4.8 的 QVariant("false").toBool() 返回的是【true】
 *   （它只把 "0"/空串当假）。那样缓存里的歌单会被全部判成"我收藏的"，
 *   分类就全塌成一组了。
 */
static bool nmPlaylistSubscribed(const QVariantMap &row)
{
    const QVariant v = row.value(QLatin1String("subscribed"));
    if (v.type() == QVariant::Bool)
        return v.toBool();

    const QString s = v.toString().trimmed().toLower();
    if (s.isEmpty())
        return false;
    return s == QLatin1String("true") || s == QLatin1String("1");
}

void MusicController::rebuildLibraryPlaylists()
{
    m_libraryPlaylists->clear();

    int mineCount = 0;
    int subscribedCount = 0;

    for (int i = 0; i < m_playlists->size(); ++i) {
        QVariantMap row = m_playlists->value(i).toMap();

        /*
         * 分两组：订阅来的（subscribed=true）和自建的。
         *
         * ★ 组的先后由 sectionKey 的字符串排序决定，这里正好不用加任何前缀：
         *     我创建的  vs  我收藏的
         *   第一个字都是"我"，比第二个字：创 U+521B < 收 U+6536，
         *   所以"我创建的"自然排在前面。
         *   （分组头直接显示这个键，所以键就是给用户看的文案。）
         */
        const bool subscribed = nmPlaylistSubscribed(row);
        if (subscribed)
            ++subscribedCount;
        else
            ++mineCount;

        row.insert(QLatin1String("sectionKey"),
                   subscribed
                   ? QLatin1String("1")     // 我收藏的
                   : QLatin1String("0"));  // 我创建的
        m_libraryPlaylists->insert(row);
    }

    /*
     * 诊断：真机日志里能直接看出分组有没有塌成一组，以及分组文案解出来是什么。
     * 正常应该是 mine=7 subscribed=9（键 0/1，由 QML 的 header 查表成"我创建的"/"我收藏的"）。
     */
    if (m_playlists->size() > 0)
        qWarning("MusicController: libraryPlaylists mine=%d subscribed=%d (keys 0/1)",
                 mineCount, subscribedCount);

    ++m_libraryPlaylistsVersion;
    emit libraryPlaylistsChanged();
}

void MusicController::loadLibraryArtist(const QString &name)
{
    const QString key = name.trimmed();
    if (key.isEmpty())
        return;

    // 从「全部歌曲」里挑出这个艺人的歌（顺序沿用列表本身的顺序）
    QVariantList songs;
    for (int i = 0; i < m_allSongMaps.size(); ++i) {
        const QVariantMap m = m_allSongMaps.at(i).toMap();
        if (!songVisible(m))
            continue;
        if (m.value(QLatin1String("artistsText")).toString().trimmed() == key)
            songs.append(m);
    }
    if (songs.isEmpty())
        return;

    /*
     * 复用播放列表页：把"这个艺人的歌"当成一个列表塞进去，
     * 并自造一份 playlistInfo（名字 = 艺人名、封面取第一首的），
     * 这样页头的封面 / 名字 / N 首 都有内容。
     * ★ id 留空：requestPlaylistComments 会因此退回"看当前曲目评论"，
     *   不会拿着假 id 去请求歌单评论。
     */
    m_lastQuery.clear();
    m_playlistInfo.clear();
    m_playlistInfo.insert(QLatin1String("id"), QString());
    m_playlistInfo.insert(QLatin1String("name"), key);
    m_playlistInfo.insert(QLatin1String("coverUrl"),
                          songs.first().toMap().value(QLatin1String("artUrl")));
    m_playlistInfo.insert(QLatin1String("trackCount"), songs.size());
    ++m_playlistInfoVersion;
    emit playlistInfoChanged();

    /*
     * ★★ 必须填【浏览模型】，绝不能 setPlaylistFrom()！
     *
     *   setPlaylistFrom 换的是全局播放队列 m_songs，而播放列表页显示的是
     *   music.browseSongs。填错模型的结果就是：页头"N 首"（取自
     *   playlistInfo.trackCount）是对的，列表里却是【上一次打开的歌单/专辑】
     *   —— 这正是真机反馈的
     *     "资料库艺术家点谁都是先前那张专辑的歌 / 歌不对但首数对"。
     *
     *   顺带也符合这个页面的定位：只是"打开来看看"，不该动播放队列。
     */
    m_browseMode = true;
    m_artistMode = false;
    m_recommendMode = false;

    m_imageCache->cancelQueued();
    m_browseSongs->clear();
    m_browseSongMaps.clear();
    for (int i = 0; i < songs.size(); ++i) {
        QVariantMap map = songs.at(i).toMap();
        map.insert(QLatin1String("localArtPath"),
                   m_imageCache->pathFor(
                       map.value(QLatin1String("artUrl")).toString()));
        map.insert(QLatin1String("type"), QLatin1String("song"));
        m_browseSongMaps.append(map);
        m_browseSongs->append(map);
    }
    ++m_browseSongsVersion;
    emit browseSongsChanged();
}

void MusicController::saveAllSongsCache()
{
    QSettings settings;
    settings.setValue(QLatin1String("nm/allsongs"), m_allSongMaps);
}

void MusicController::loadAllSongsCache()
{
    QSettings settings;
    const QVariantList list = settings.value(QLatin1String("nm/allsongs")).toList();
    if (list.isEmpty())
        return;
    m_allSongMaps = list;
    rebuildAllSongs();
}

int MusicController::artistCount() const
{
    return m_artists->size();
}

void MusicController::snapshotList()
{
    m_snapshotSongs = m_songMaps;
    m_snapshotAlbums.clear();
    for (int i = 0; i < m_albums->size(); ++i)
        m_snapshotAlbums.append(m_albums->value(i));
    m_snapshotPlaylistInfo = m_playlistInfo;
    m_snapshotLastQuery = m_lastQuery;
    m_hasSnapshot = true;
    // 重新开始观察：进这个页面后用户有没有真的播过
    m_queueAdopted = false;
}

void MusicController::restoreList()
{
    if (!m_hasSnapshot)
        return;
    m_hasSnapshot = false;

    /*
     * ★ 用户在这个页面里点过播放 → 当前列表已经是他选的队列了，
     *   【不能还原】，否则一返回队列就被退回进页面前的样子，正在播的
     *   那首跟队列对不上（next/prev 会跳到别的列表去）。
     */
    if (m_queueAdopted) {
        m_queueAdopted = false;
        return;
    }

    m_songMaps = m_snapshotSongs;
    m_songs->clear();
    foreach (const QVariant &item, m_snapshotSongs) {
        if (songVisible(item.toMap()))
            m_songs->append(item);
    }

    /*
     * ★ 还原后 currentIndex 要重新定位到【正在播的那首】在还原列表里的位置。
     *   还原前的下标属于被浏览的那个列表，留着的话 next/prev 会走错地方。
     *   找不到（正在播的歌不在还原后的列表里）就置 -1，next() 会从 0 开始。
     */
    m_currentIndex = -1;
    const QString curId = m_currentTrack.value(QLatin1String("id")).toString();
    if (!curId.isEmpty()) {
        for (int i = 0; i < m_songs->size(); ++i) {
            if (m_songs->value(i).toMap().value(QLatin1String("id")).toString() == curId) {
                m_currentIndex = i;
                break;
            }
        }
    }

    m_albums->clear();
    foreach (const QVariant &item, m_snapshotAlbums)
        m_albums->append(item);

    m_playlistInfo = m_snapshotPlaylistInfo;
    m_lastQuery = m_snapshotLastQuery;

    ++m_albumsVersion;
    ++m_playlistInfoVersion;
    emit songsChanged();
    emit albumsChanged();
    emit playlistInfoChanged();
}

/*!
 * 解析一个 LRC 时间标签，如 "00:12.34" / "00:12" → 毫秒。
 * 手写解析（不用 QRegExp），省得为一个标签再引一个头文件。
 */
/*! 歌词行按时间升序（qSort 用；不用 C++11 lambda，兼容老工具链） */
static bool lyricPairLess(const QPair<int, QString> &a,
                          const QPair<int, QString> &b)
{
    return a.first < b.first;
}

static bool parseLrcStamp(const QString &s, int *msec)
{
    const int colon = s.indexOf(QLatin1Char(':'));
    if (colon <= 0)
        return false;

    const int min = s.left(colon).toInt();
    const QString rest = s.mid(colon + 1);
    const int dot = rest.indexOf(QLatin1Char('.'));
    const QString secStr = (dot >= 0) ? rest.left(dot) : rest;
    const int sec = secStr.toInt();

    int frac = 0;
    if (dot >= 0) {
        QString f = rest.mid(dot + 1);
        while (f.length() < 3)
            f.append(QLatin1Char('0'));
        frac = f.left(3).toInt();
    }

    *msec = (min * 60 + sec) * 1000 + frac;
    return true;
}

/*
 * ============================================================================
 *  歌词开头「元信息」清理
 *
 *  规则移植自参考项目 ColorOS-Live-Lyrics-Bridge 的三个类：
 *    LyricMetadataFilter / LyricOpeningCleanup / LockscreenIntegrationPolicy
 *  （按用户要求：默认全清，不做开关）
 *
 *    1) 制作人员行 —— 作词 / 作曲 / 编曲 / 制作人 / 混音 / 母带 / 录音 /
 *       吉他 / 乐队 / 指挥 …，以及英文的 Lyrics by / Composer /
 *       Produced by / Mixed by / Piano / Guitar / Orchestra …
 *    2) 版权与权利声明 —— © / copyright / all rights reserved /
 *       版权所有 / 未经许可 …
 *    3) 开头那行「歌名 - 歌手」。
 *    4) 纯音乐占位文案 —— 纯音乐 / 暂无歌词 / instrumental …
 *       （这类本身就是「没歌词」的标志，清掉后界面自然回落成歌名-歌手）
 *
 *  ★ 1)2)3) 只在【开头】生效（前 32 行且时间 <= 30s，和参考实现一致），
 *    免得误伤正文里恰好含这些词的歌词；4) 本身就是占位文案，任何位置都清。
 * ============================================================================
 */

/*! 版权 / 权利声明行 */
static bool isLyricCopyrightLine(const QString &text)
{
    const QString t = text.trimmed();
    if (t.isEmpty())
        return false;
    if (t.startsWith(QChar(0x00A9)))   // ©
        return true;

    const QString lower = t.toLower();
    static const char *kEn[] = { "copyright", "all rights reserved",
                                 "used by permission" };
    for (int i = 0; i < (int)(sizeof(kEn) / sizeof(kEn[0])); ++i) {
        if (lower.contains(QLatin1String(kEn[i])))
            return true;
    }

    static const char *kCjk[] = { "版权所有", "著作权", "未经许可",
                                  "未经授权", "翻译作品" };
    for (int i = 0; i < (int)(sizeof(kCjk) / sizeof(kCjk[0])); ++i) {
        if (t.contains(QString::fromUtf8(kCjk[i])))
            return true;
    }
    return false;
}

/*! 纯音乐 / 无歌词 这类占位文案（清掉后各页会回落成歌名 - 歌手） */
static bool isLyricPlaceholderLine(const QString &text)
{
    const QString t = text.trimmed();
    // 占位文案都很短；限长是为了不误伤正文里恰好含"纯音乐"三个字的长句
    if (t.isEmpty() || t.length() > 64)
        return false;

    static const char *kWords[] = {
        "纯音乐", "暂无歌词", "无歌词", "没有填词", "此歌曲为",
        "instrumental", "no lyrics"
    };
    const QString lower = t.toLower();
    for (int i = 0; i < (int)(sizeof(kWords) / sizeof(kWords[0])); ++i) {
        const QString w = QString::fromUtf8(kWords[i]);
        if (t.contains(w) || lower.contains(w))
            return true;
    }
    return false;
}

/*! 制作人员 / 乐器 / 乐团 行（只看冒号前的 label） */
static bool isLyricProductionLine(const QString &text)
{
    const QString t = text.trimmed();
    if (t.isEmpty())
        return false;

    int sep = t.indexOf(QLatin1Char(':'));
    if (sep < 0)
        sep = t.indexOf(QString::fromUtf8("："));
    const QString label = (sep >= 0 ? t.left(sep) : t).trimmed().toLower();
    if (label.isEmpty() || label.length() > 40)
        return false;

    static const char *kEn[] = {
        "lyrics by", "lyric by", "written by", "composed by", "composer",
        "produced by", "producer", "arranged by", "performed by", "mixed by",
        "mastered by", "recorded by", "engineered by", "vocals by",
        "vocals recorded", "background vocal", "backing vocal", "orchestration",
        "percussion", "synth", "synthesizer", "viola", "violin", "piano",
        "acoustic guitar", "electric guitar", "drum", "digital edit",
        "mixed in dolby atmos", "orchestra", "band", "choir", "conductor",
        "accordion", "strings", "guitar", "bass", "cello",
        "original publisher", "sub-publisher", "publisher"
    };
    for (int i = 0; i < (int)(sizeof(kEn) / sizeof(kEn[0])); ++i) {
        const QString c = QLatin1String(kEn[i]);
        if (label == c || label.startsWith(c + QLatin1Char(' ')))
            return true;
    }

    /*
     * ★ 没有冒号就到此为止 —— 中文职员表几乎都写成「作词：XXX」，一定有冒号；
     *   而正文里一句"鼓点响起"没有冒号，若还拿"鼓"去 contains 就会误杀歌词。
     *   所以无冒号时只认上面那张英文职员表（Composer / Piano / Produced by …）。
     */
    if (sep < 0)
        return false;

    static const char *kCjk[] = {
        "作词", "作曲", "编曲", "制作", "演唱", "歌手", "原唱", "翻唱",
        "混音", "母带", "录音", "监制", "配唱", "人声", "吉他", "贝斯",
        "鼓", "和音", "监棚", "弦乐", "和声", "乐谱",
        "乐队", "管弦乐", "交响乐团", "合唱", "指挥", "手风琴", "钢琴",
        "大提琴", "小提琴", "出品", "发行"
    };
    for (int i = 0; i < (int)(sizeof(kCjk) / sizeof(kCjk[0])); ++i) {
        if (label.contains(QString::fromUtf8(kCjk[i])))
            return true;
    }

    return label == QString::fromUtf8("词") || label == QString::fromUtf8("曲");
}

/*! 字符串里有没有字母（中英文都算），用于排除"哎 - 呀"这类真歌词 */
static bool lyricHasLetter(const QString &v)
{
    for (int i = 0; i < v.length(); ++i) {
        if (v.at(i).isLetter())
            return true;
    }
    return false;
}

/*! 开头那行「歌名 - 歌手」 */
static bool isLyricTitleArtistLine(const QString &text, int msec)
{
    if (msec < 0 || msec > 15000)
        return false;

    const QString t = text.trimmed();
    if (t.isEmpty() || t.length() > 96)
        return false;

    // 带句末标点的更像正文歌词，放过
    const QChar last = t.at(t.length() - 1);
    if (last == QLatin1Char('.') || last == QLatin1Char('!')
            || last == QLatin1Char('?') || last == QChar(0x3002)
            || last == QChar(0xFF01) || last == QChar(0xFF1F))
        return false;

    const int sep = t.indexOf(QLatin1String(" - "));
    if (sep <= 0)
        return false;

    const QString title = t.left(sep).trimmed();
    const QString artist = t.mid(sep + 3).trimmed();
    if (title.length() < 2 || artist.length() < 2
            || title.length() > 48 || artist.length() > 48)
        return false;

    return lyricHasLetter(title) && lyricHasLetter(artist);
}

/*!
 * 这一句要不要在显示前丢掉。
 * @param lineIndex 物理行号（用于"只在开头"的范围判断）
 * @param msec      该行的时间戳
 */
static bool shouldDropLyricLine(const QString &text, int msec, int lineIndex)
{
    // 占位文案（纯音乐 / 暂无歌词）：本身就是"没歌词"的标志，任何位置都清
    if (isLyricPlaceholderLine(text))
        return true;

    // 其余元信息只在开头清（照参考实现：前 32 行且 <= 30s）
    if (lineIndex >= 32 || msec > 30000)
        return false;

    if (isLyricCopyrightLine(text))
        return true;
    if (isLyricProductionLine(text))
        return true;
    if (isLyricTitleArtistLine(text, msec))
        return true;

    return false;
}

void MusicController::onLyricFinished(int requestId,
                                      const nm::NmParsers::LyricParse &result)
{
    Q_UNUSED(requestId);

    /*
     * ★ 诊断放在【最前面】：之前它在 `if (!result.ok) return;` 之后，
     *   一旦 ok=false 就直接 return，日志里连这行都看不到，没法判断
     *   到底是"没回调"还是"回调了但没数据"。
     */
    qWarning("MusicController: onLyricFinished ok=%d code=%d lrc=%d trans=%d",
             result.ok ? 1 : 0, result.code,
             result.lyric.size(), result.trans.size());

    if (!result.ok)
        return;

    /*
     * ★★ 分流：这次请求如果是【详情页】替列表里某首歌发的，就只把原文存进
     *   m_propertiesLyric —— 绝不动 m_lyricRaw / m_lyricTimes。
     *   那些是主界面迷你条正在用的歌词，顶掉就会串词。
     */
    if (m_propertiesLyricFetching) {
        m_propertiesLyricFetching = false;
        m_propertiesLyric = result.lyric;
        ++m_propertiesVersion;
        emit propertiesChanged();
        return;
    }

    m_lyricRaw = result.lyric;
    m_transRaw = result.trans;

    // 收集 (时间, 文本)，最后按时间排序（一行可能有多个时间标签，未必有序）
    QList<QPair<int, QString> > items;
    const QStringList lines = m_lyricRaw.split(QLatin1Char('\n'));
    for (int li = 0; li < lines.size(); ++li) {
        const QString &line = lines.at(li);
        int pos = 0;
        QList<int> stamps;
        while (pos < line.length()) {
            const int lb = line.indexOf(QLatin1Char('['), pos);
            if (lb < 0)
                break;
            const int rb = line.indexOf(QLatin1Char(']'), lb + 1);
            if (rb < 0)
                break;
            int ms = 0;
            if (parseLrcStamp(line.mid(lb + 1, rb - lb - 1), &ms))
                stamps.append(ms);
            pos = rb + 1;
        }
        if (stamps.isEmpty())
            continue;
        const QString text = line.mid(pos).trimmed();
        /*
         * ★ 只有时间标签、没文本的【间奏空行】直接跳过，别当成一句空歌词塞进去。
         *   否则 updateLyricForPosition 会取到这条空串把歌词清掉 ——
         *   表现就是"一到间奏标题就闪回歌名"。
         *   跳过之后，间奏期间取到的仍是【上一句】歌词（用户要的效果）。
         */
        if (text.isEmpty())
            continue;
        /*
         * ★ 丢掉开头的 作词/作曲/版权/「歌名 - 歌手」 以及纯音乐占位文案
         *   （见 shouldDropLyricLine）。
         *   清掉之后，纯音乐那种歌就【一行歌词都不剩】—— 界面自然回落成
         *   歌名 - 歌手，而不是把"纯音乐，请欣赏"当歌词显示出来。
         */
        if (shouldDropLyricLine(text, stamps.first(), li))
            continue;
        foreach (int t, stamps)
            items.append(qMakePair(t, text));
    }

    qWarning("MusicController: lyrics rawRows=%d timedLines=%d",
             lines.size(), items.size());

    qSort(items.begin(), items.end(), lyricPairLess);

    m_lyricTimes.clear();
    m_lyricTexts.clear();
    // ★ 用下标循环：foreach 宏遇到 QPair<int, QString> 里的逗号会被当成参数分隔符
    for (int i = 0; i < items.size(); ++i) {
        m_lyricTimes.append(items.at(i).first);
        m_lyricTexts.append(items.at(i).second);
    }

    /*
     * ★ 翻译：解析 m_transRaw 并对齐到每一句原词。
     *
     *   ⚠ 这一整段之前【没落盘】—— m_transRaw 赋了值却从没被解析，
     *     m_lyricTrans 永远是空，所以"点开翻译但副标题没变"。
     *
     *   ⚠ 也不能拿原词时间【精确查表】：网易的 tlyric 时间戳不保证逐句对齐
     *     （有的不带毫秒、有的整体错开），精确匹配一句都中不了。
     *     改成「取时间 <= 该句时间的最后一条翻译」。
     */
    m_lyricTrans.clear();
    if (m_transRaw.isEmpty()) {
        for (int i = 0; i < m_lyricTimes.size(); ++i)
            m_lyricTrans.append(QString());
    } else {
        QList<QPair<int, QString> > titems;
        const QStringList tlines = m_transRaw.split(QLatin1Char('\n'));
        foreach (const QString &line, tlines) {
            int tpos = 0;
            QList<int> stamps;
            while (tpos < line.length()) {
                const int lb = line.indexOf(QLatin1Char('['), tpos);
                if (lb < 0)
                    break;
                const int rb = line.indexOf(QLatin1Char(']'), lb + 1);
                if (rb < 0)
                    break;
                int ms = 0;
                if (parseLrcStamp(line.mid(lb + 1, rb - lb - 1), &ms))
                    stamps.append(ms);
                tpos = rb + 1;
            }
            if (stamps.isEmpty())
                continue;
            const QString text = line.mid(tpos).trimmed();
            foreach (int t, stamps)
                titems.append(qMakePair(t, text));
        }

        qSort(titems.begin(), titems.end(), lyricPairLess);

        int k = 0;
        QString cur;
        for (int i = 0; i < m_lyricTimes.size(); ++i) {
            const int t = m_lyricTimes.at(i);
            while (k < titems.size() && titems.at(k).first <= t) {
                cur = titems.at(k).second;
                ++k;
            }
            m_lyricTrans.append(cur);
        }
    }

    // 立刻按当前位置同步一次（歌词常比进度晚到）
    updateLyricForPosition(m_playerPosition);
}

void MusicController::updateLyricForPosition(int msec)
{
    QString next;
    QString nextTrans;
    for (int i = 0; i < m_lyricTimes.size(); ++i) {
        if (m_lyricTimes.at(i) <= msec) {
            next = m_lyricTexts.at(i);
            nextTrans = (i < m_lyricTrans.size()) ? m_lyricTrans.at(i)
                                                  : QString();
        } else {
            break;
        }
    }

    if (next != m_currentLyricLine) {
        m_currentLyricLine = next;
        emit currentLyricLineChanged();
        emit coverInfoChanged();
    }
    if (nextTrans != m_currentLyricTrans) {
        m_currentLyricTrans = nextTrans;
        emit currentLyricTransChanged();
        emit coverInfoChanged();
    }
}

void MusicController::clearBrowseList()
{
    /*
     * 只清【浏览列表】 m_browseSongs，绝不动 m_songs（播放队列）。
     * 用途：打开歌单 / 专辑 / 搜索时先清一遍，避免加载期间残留上一个列表。
     * （原来由 clearSongs() 兼任，但它会连队列一起清，所以拆出来单独做。）
     */
    m_browseSongs->clear();
    m_browseSongMaps.clear();
    ++m_browseSongsVersion;
    emit browseSongsChanged();
}

bool MusicController::loadPlaylistSongsCache(const QString &playlistId)
{
    if (playlistId.isEmpty())
        return false;

    QSettings settings;
    const QVariantMap entry =
        settings.value(QLatin1String("nm/plsongs/") + playlistId).toMap();
    /*
     * ★★ 校验缓存里记的歌单 id。
     *   老数据没有这个字段（storedId 为空）→ 一律当【脏数据】丢弃：
     *   它们是拿"播放队列"写出来的（见 savePlaylistSongsCache 的说明），
     *   内容往往属于别的歌单 —— 用它会先显示上一个歌单的内容再闪一下。
     *   丢弃并顺手删掉，下次写入的才是干净的（带 id）。
     */
    const QString storedId = entry.value(QLatin1String("id")).toString();
    if (storedId != playlistId) {
        qWarning("MusicController: playlist cache STALE id=%s stored=%s -> 丢弃",
                 qPrintable(playlistId), qPrintable(storedId));
        settings.remove(QLatin1String("nm/plsongs/") + playlistId);
        return false;
    }

    const QVariantList list = entry.value(QLatin1String("songs")).toList();
    if (list.isEmpty()) {
        qWarning("MusicController: playlist cache MISS id=%s", qPrintable(playlistId));
        return false;
    }

    qWarning("MusicController: playlist cache HIT id=%s count=%d",
             qPrintable(playlistId), list.size());

    // 标题也先用缓存里的（网络回来前先显示对的名字）
    if (m_playlistInfo.isEmpty()) {
        const QString name = entry.value(QLatin1String("name")).toString();
        if (!name.isEmpty()) {
            m_playlistInfo.insert(QLatin1String("name"), name);
            ++m_playlistInfoVersion;
            emit playlistInfoChanged();
        }
    }

    m_imageCache->cancelQueued();

    /*
     * ★ 目标模型必须走 songsTarget()：浏览模式下填【浏览列表】。
     *   原来这里直接写 m_songs —— 那就是播放队列，缓存一命中就把队列
     *   整个换掉了（"打开歌单队列就变了"的元凶之一）。
     */
    bb::cascades::ArrayDataModel *model = songsTarget();
    QVariantList &maps = songsTargetMaps();

    model->clear();
    maps.clear();
    foreach (const QVariant &item, list) {
        QVariantMap map = item.toMap();
        map.insert(QLatin1String("localArtPath"),
                   m_imageCache->pathFor(map.value(QLatin1String("artUrl")).toString()));
        map.insert(QLatin1String("type"), QLatin1String("song"));
        maps.append(map);
        if (songVisible(map))
            model->append(map);
    }

    if (m_browseMode)
        emit browseSongsChanged();
    else
        emit songsChanged();

    // 专辑索引只在【队列】模式下才需要跟着建，浏览时不动它
    if (!m_browseMode)
        buildAlbumsFromSongs();

    return true;
}

void MusicController::savePlaylistSongsCache(const QString &playlistId)
{
    /*
     * ★★ 要存的是【浏览列表 m_browseSongMaps】，不是 m_songMaps！
     *   m_songMaps 是【播放队列】—— 用户播过哪张歌单，它就是那张的内容。
     *   拿它当"这个歌单的曲目"存下来，就会出现：
     *     播过 655 首的大歌单 → 再点一个 10 首的小歌单 →
     *     小歌单的缓存里存进去的是那 655 首（真机日志：cache HIT count=655，
     *     网络回来却只有 10 首）→ 进去先显示上一个歌单的内容、再闪成自己的。
     *   浏览列表才是"刚刚打开的这个歌单真正的内容"。
     */
    if (playlistId.isEmpty() || m_browseSongMaps.isEmpty())
        return;

    QSettings settings;

    // 最多留 8 个歌单，超了按 LRU 删
    QStringList ids = settings.value(QLatin1String("nm/plsongs_ids")).toStringList();
    ids.removeAll(playlistId);
    ids.prepend(playlistId);
    while (ids.size() > 8) {
        const QString old = ids.takeLast();
        settings.remove(QLatin1String("nm/plsongs/") + old);
    }
    settings.setValue(QLatin1String("nm/plsongs_ids"), ids);

    // 存精简副本（去掉本地路径字段，下次重新算）
    QVariantList list;
    for (int i = 0; i < m_browseSongMaps.size(); ++i) {
        QVariantMap map = m_browseSongMaps.at(i).toMap();
        map.remove(QLatin1String("localArtPath"));
        list.append(map);
    }

    QVariantMap entry;
    // 连歌单 id 一起存：读的时候校验，历史脏数据（对不上）直接作废
    entry.insert(QLatin1String("id"), playlistId);
    entry.insert(QLatin1String("name"), m_playlistInfo.value(QLatin1String("name")));
    entry.insert(QLatin1String("songs"), list);
    settings.setValue(QLatin1String("nm/plsongs/") + playlistId, entry);
}

void MusicController::setLoading(bool value)
{
    if (value == m_loading)
        return;
    m_loading = value;
    emit loadingChanged();
}

void MusicController::setError(const QString &message, const QVariantMap &detail)
{
    m_errorMessage = message;
    m_errorDetail = detail;
    emit errorMessageChanged();
    emit errorDetailChanged();
}

void MusicController::finishRequest(const QString &what, bool success)
{
    setLoading(false);
    emit requestFinished(what, success);
}

void MusicController::fillSongsModel(const QList<nm::NmSong> &songs)
{
    // 旧列表排队中的封面先取消，别占着队列（见 NmImageCache::cancelQueued）
    m_imageCache->cancelQueued();

    // 诊断：看每次到底填的是哪个模型（定位"推荐页被搜索污染"这类问题）
    qWarning("MusicController: fillSongsModel artist=%d recommend=%d count=%d",
             m_artistMode ? 1 : 0, m_recommendMode ? 1 : 0, songs.size());

    // 目标模型：艺人模式 → artistSongs，推荐模式 → recommendSongs，否则 → 全局 songs
    ArrayDataModel *model = songsTarget();
    QVariantList &maps = songsTargetMaps();

    // 先装进临时列表，和手上现有的比一比再决定要不要重建
    QVariantList next;
    foreach (const nm::NmSong &song, songs) {
        QVariantMap map = song.toVariantMap();
        // 封面本地路径：没下完是空串（QML 里 ImageView 就空着），
        // pathFor 会顺手把没缓存的入队下载。
        map.insert(QLatin1String("localArtPath"),
                   m_imageCache->pathFor(song.artUrl));
        // type：给 ListView 的 ListItemComponent 分派用（ArrayDataModel
        // 的项本身没有类型概念，只能自己带一个字段）
        map.insert(QLatin1String("type"), QLatin1String("song"));
        next.append(map);
    }

    /*
     * ★ 内容一致就【不重建】。
     *   进歌单 / 专辑时这份列表会填两遍：先本地缓存填一次（界面立刻有内容），
     *   网络回来 fill 又填一次。每次都 clear + 全量重插 + 排序，界面上就是
     *   "点进去一秒后闪一下"（用户反馈）。
     *   id 序列一致就认为内容没变 —— 直接跳过，不动模型；
     *   localArtPath 的差异由 refreshImagePaths() 负责补。
     */
    if (sameSongMaps(maps, next)) {
        if (m_artistMode)
            buildArtistAlbums();
        else if (!m_browseMode && !m_recommendMode)
            buildAlbumsFromSongs();
        return;
    }

    model->clear();
    maps = next;
    // ★ maps 始终保持【完整】（「隐藏 VIP」开关靠它重填，无需重新联网）；
    //   只有显示模型按开关过滤掉 VIP 曲目。
    for (int i = 0; i < maps.size(); ++i) {
        if (songVisible(maps.at(i).toMap()))
            model->append(maps.at(i));
    }

    if (m_artistMode) {
        // 艺人页只是浏览，别动全局 songs / 播放状态
        buildArtistAlbums();
        ++m_artistSongsVersion;
        ++m_artistAlbumsVersion;
        emit artistSongsChanged();
        return;
    }
    if (m_browseMode) {
        /*
         * ★ 歌单 / 专辑 / 搜索结果【只是打开来看】：进独立浏览模型，
         *   全局 m_songs（= 播放队列）完全不动。
         *   只有用户点某首（playBrowseSong）才把它拷成队列。
         */
        ++m_browseSongsVersion;
        emit browseSongsChanged();
        return;
    }
    if (m_recommendMode) {
        // 每日推荐进独立模型，别动全局 songs（否则搜索会"污染"推荐页）
        ++m_recommendSongsVersion;
        emit recommendSongsChanged();
        return;
    }

    resetPlayer();
    emit songsChanged();

    // 专辑页的数据来自本地聚合（见 albums 属性的说明）
    buildAlbumsFromSongs();
}

void MusicController::buildArtistAlbums()
{
    m_artistAlbums->clear();

    QHash<qint64, QVariantMap> byId;
    QList<qint64> order;

    for (int i = 0; i < m_artistSongMaps.size(); ++i) {
        const QVariantMap song = m_artistSongMaps.at(i).toMap();
        const qint64 albumId = song.value(QLatin1String("albumId")).toString().toLongLong();
        const QString albumName = song.value(QLatin1String("albumName")).toString();
        if (albumId <= 0 || albumName.isEmpty())
            continue;

        if (!byId.contains(albumId)) {
            QVariantMap album;
            album.insert(QLatin1String("id"), QString::number(albumId));
            album.insert(QLatin1String("name"), albumName);
            album.insert(QLatin1String("artist"), song.value(QLatin1String("artistsText")));
            album.insert(QLatin1String("artUrl"), song.value(QLatin1String("artUrl")));
            album.insert(QLatin1String("trackCount"), 0);
            byId.insert(albumId, album);
            order.append(albumId);
        }

        QVariantMap album = byId.value(albumId);
        album.insert(QLatin1String("trackCount"),
                     album.value(QLatin1String("trackCount")).toInt() + 1);
        if (album.value(QLatin1String("artUrl")).toString().isEmpty()) {
            const QString art = song.value(QLatin1String("artUrl")).toString();
            if (!art.isEmpty())
                album.insert(QLatin1String("artUrl"), art);
        }
        byId.insert(albumId, album);
    }

    foreach (qint64 albumId, order) {
        QVariantMap album = byId.value(albumId);
        album.insert(QLatin1String("localCoverPath"),
                     m_imageCache->pathFor(album.value(QLatin1String("artUrl")).toString()));
        album.insert(QLatin1String("type"), QLatin1String("album"));
        m_artistAlbums->append(album);
    }
}

void MusicController::loadArtist(const QString &name)
{
    m_imageCache->cancelQueued();
    m_artistSongs->clear();
    m_artistSongMaps.clear();
    m_artistAlbums->clear();
    ++m_artistSongsVersion;
    ++m_artistAlbumsVersion;
    emit artistSongsChanged();

    /*
     * ★ 先停掉搜索防抖定时器！
     *   否则用户在搜索框打过字、防抖还没到点时进艺人页，
     *   稍后 onSearchDebounce() 触发会把 m_artistMode 清成 false，
     *   艺人的搜索结果就被填进浏览列表 —— 表现就是"艺人页有时有内容、
     *   有时空的"，专辑页（由艺人歌曲聚合）也跟着一起空。
     */
    if (m_searchTimer)
        m_searchTimer->stop();

    m_artistMode = true;
    setLoading(true);
    m_api->searchSongs(name, 30);
}

void MusicController::playArtistSong(int index)
{
    // ★ 用【显示的模型】而非完整 maps：隐藏 VIP 时下标才对得上
    if (index < 0 || index >= m_artistSongs->size())
        return;
    QVariantList shown;
    for (int i = 0; i < m_artistSongs->size(); ++i)
        shown.append(m_artistSongs->value(i));
    setPlaylistFrom(shown);
    m_artistMode = false;       // 之后都走全局
    playIndex(index);
}

void MusicController::playRecommendSong(int index)
{
    // ★ 用【显示的模型】而非完整 maps：隐藏 VIP 时下标才对得上
    if (index < 0 || index >= m_recommendSongs->size())
        return;
    QVariantList shown;
    for (int i = 0; i < m_recommendSongs->size(); ++i)
        shown.append(m_recommendSongs->value(i));
    setPlaylistFrom(shown);
    m_recommendMode = false;
    playIndex(index);
}

void MusicController::setPlaylistFrom(const QVariantList &maps)
{
    // 把拷过来的列表当当前播放列表（next/prev 要用 m_songMaps）
    m_imageCache->cancelQueued();
    // ★ 同上：整份列表被拿去当队列 = 已接管
    m_queueAdopted = true;
    m_songs->clear();
    m_songMaps.clear();
    foreach (const QVariant &item, maps) {
        const QVariantMap map = item.toMap();
        m_songMaps.append(map);
        if (songVisible(map))
            m_songs->append(map);
    }
    resetPlayer();
    emit songsChanged();
    buildAlbumsFromSongs();
}

void MusicController::buildAlbumsFromSongs()
{
    m_albums->clear();

    // albumId -> 聚合结果（保持首次出现顺序，用 QList 记录顺序）
    QHash<qint64, QVariantMap> byId;
    QList<qint64> order;

    for (int i = 0; i < m_songMaps.size(); ++i) {
        const QVariantMap song = m_songMaps.at(i).toMap();
        const qint64 albumId = song.value(QLatin1String("albumId")).toString().toLongLong();
        const QString albumName = song.value(QLatin1String("albumName")).toString();
        if (albumId <= 0 || albumName.isEmpty())
            continue;

        if (!byId.contains(albumId)) {
            QVariantMap album;
            album.insert(QLatin1String("id"), QString::number(albumId));
            album.insert(QLatin1String("name"), albumName);
            album.insert(QLatin1String("artist"), song.value(QLatin1String("artistsText")));
            album.insert(QLatin1String("artUrl"), song.value(QLatin1String("artUrl")));
            album.insert(QLatin1String("trackCount"), 0);
            byId.insert(albumId, album);
            order.append(albumId);
        }

        QVariantMap album = byId.value(albumId);
        album.insert(QLatin1String("trackCount"),
                     album.value(QLatin1String("trackCount")).toInt() + 1);
        // 前面几首可能还没补齐封面，后面补上了就用有封面的那份
        if (album.value(QLatin1String("artUrl")).toString().isEmpty()) {
            const QString art = song.value(QLatin1String("artUrl")).toString();
            if (!art.isEmpty())
                album.insert(QLatin1String("artUrl"), art);
        }
        byId.insert(albumId, album);
    }

    foreach (qint64 albumId, order) {
        QVariantMap album = byId.value(albumId);
        album.insert(QLatin1String("localCoverPath"),
                     m_imageCache->pathFor(album.value(QLatin1String("artUrl")).toString()));
        album.insert(QLatin1String("type"), QLatin1String("album"));
        m_albums->append(album);
    }

    ++m_albumsVersion;
    emit albumsChanged();
}

void MusicController::resetPlayer()
{
    m_playUrl.clear();
    ++m_playUrlVersion;
    emit playUrlChanged();
}

void MusicController::onSearchFinished(int requestId,
                                       const nm::NmParsers::SearchParse &result)
{
    Q_UNUSED(requestId);

    /*
     * ★ 诊断放在【最前面】：之前放在 !result.ok 之后，解析失败时直接 return
     *   就什么都看不到，才导致定位不了。开「输出控制台日志」可见。
     */
    qWarning("MusicController: onSearchFinished ok=%d songs=%d code=%d artist=%d browse=%d",
             result.ok ? 1 : 0, result.songs.size(), result.code,
             m_artistMode ? 1 : 0, m_browseMode ? 1 : 0);

    if (!result.ok) {
        setError(result.error);
        finishRequest(QLatin1String("search"), false);
        return;
    }

    /*
     * ★ 诊断：搜索不出东西时靠这条判断是"接口没返回"还是"填模型出问题"。
     *   开「输出控制台日志」可见。
     */
    qWarning("MusicController: onSearchFinished ok=%d songs=%d browse=%d",
             result.ok ? 1 : 0, result.songs.size(), m_browseMode ? 1 : 0);

    fillSongsModel(result.songs);

    /*
     * ★ 搜索页有【独立】模型。全局 songs 会被"播放任意一首歌"时的
     *   setPlaylistFrom() 换成那首歌所在的歌单/专辑 —— 搜索页若绑全局，
     *   一进去看到的就全是"当前歌相关的列表"（真机反馈）。
     *   这里把刚填好的搜索结果另拷一份给搜索页专用模型。
     */
    /*
     * ★ 搜索结果现在填在【浏览模型】里（不动播放队列），镜像也从目标模型取。
     *
     * ★★ 但只有【普通搜索】才镜像！艺人页（loadArtist）走的也是
     *   m_api->searchSongs()，若无条件镜像，一进艺人页搜索页就被
     *   "这个艺人的歌"占满（真机反馈："点了艺人，资料库搜索项全变成他的歌"）。
     */
    if (!m_artistMode) {
        m_searchSongs->clear();
        bb::cascades::ArrayDataModel *filled = songsTarget();
        for (int i = 0; i < filled->size(); ++i)
            m_searchSongs->append(filled->value(i));
        ++m_searchSongsVersion;
        emit searchSongsChanged();
    }

    // 补齐封面（见 requestArtEnrichmentIfNeeded 的说明）
    if (requestArtEnrichmentIfNeeded())
        return;                     // 等 songDetail 回来再收尾

    if (result.songs.isEmpty())
        setError(NmTr("E6B2A1E69C89E689BEE588B0E79BB8E585B3E6AD8CE69BB2"));

    finishRequest(QLatin1String("search"), true);
}

void MusicController::onSongDetailFinished(
        int requestId, const nm::NmParsers::SongDetailParse &result)
{
    Q_UNUSED(requestId);

    // 补齐失败不影响搜索本身：列表已经显示出来了，只是没封面
    if (result.ok)
        mergeArtUrls(result.songs);

    m_artistMode = false;
    m_recommendMode = false;
    finishRequest(QLatin1String("search"), true);
}

bool MusicController::requestArtEnrichmentIfNeeded()
{
    /*
     * ★ 有些接口（/api/search/get/web）只给 album.picId 不给 picUrl，
     *   列表这时是没有封面的。picId 无法自己拼出 CDN 地址
     *   （试过 p1~p4 直拼全是 404），只能再走一次 /api/song/detail，
     *   它才返回可用的 album.picUrl。一次请求批量补齐整页。
     */
    const QVariantList &maps = songsTargetMaps();
    QList<qint64> missingIds;
    for (int i = 0; i < maps.size(); ++i) {
        const QVariantMap map = maps.at(i).toMap();
        if (map.value(QLatin1String("artUrl")).toString().isEmpty()) {
            const qint64 id = map.value(QLatin1String("id")).toString().toLongLong();
            if (id > 0)
                missingIds.append(id);
        }
    }

    if (missingIds.isEmpty())
        return false;

    m_api->fetchSongDetail(missingIds);
    return true;
}

void MusicController::mergeArtUrls(const QList<nm::NmSong> &details)
{
    if (details.isEmpty())
        return;

    // id -> picUrl
    QHash<qint64, QString> artUrls;
    foreach (const nm::NmSong &song, details) {
        if (song.id > 0 && !song.artUrl.isEmpty())
            artUrls.insert(song.id, song.artUrl);
    }
    if (artUrls.isEmpty())
        return;

    ArrayDataModel *model = songsTarget();
    QVariantList &maps = songsTargetMaps();

    for (int i = 0; i < model->size(); ++i) {
        QVariantMap map = model->value(i).toMap();
        const qint64 id = map.value(QLatin1String("id")).toString().toLongLong();
        if (!artUrls.contains(id))
            continue;

        const QString artUrl = artUrls.value(id);
        if (map.value(QLatin1String("artUrl")).toString() == artUrl)
            continue;

        map.insert(QLatin1String("artUrl"), artUrl);
        // 顺手发起下载；下完后 refreshImagePaths 会填 localArtPath
        map.insert(QLatin1String("localArtPath"), m_imageCache->pathFor(artUrl));
        if (i < model->size())
            model->replace(i, map);
    }

    // maps 是 next/prev 和「正在播放」的数据源，同步更新
    for (int i = 0; i < maps.size(); ++i) {
        QVariantMap map = maps.at(i).toMap();
        const qint64 id = map.value(QLatin1String("id")).toString().toLongLong();
        if (!artUrls.contains(id))
            continue;
        const QString artUrl = artUrls.value(id);
        map.insert(QLatin1String("artUrl"), artUrl);
        map.insert(QLatin1String("localArtPath"), m_imageCache->pathFor(artUrl));
        maps.replace(i, map);
    }

    if (m_artistMode) {
        // 艺人页：专辑封面也重建
        buildArtistAlbums();
        ++m_artistSongsVersion;
        ++m_artistAlbumsVersion;
        emit artistSongsChanged();
        return;
    }
    if (m_recommendMode) {
        ++m_recommendSongsVersion;
        emit recommendSongsChanged();
        return;
    }

    // currentTrack 是播放时从 m_songMaps 拷的快照，也要跟着更新
    if (m_currentIndex >= 0 && m_currentIndex < m_songMaps.size()) {
        m_currentTrack = m_songMaps.at(m_currentIndex).toMap();
        ++m_currentTrackVersion;
        emit currentTrackChanged();
    emit coverInfoChanged();
    /*
     * ★ 再补发一次（800ms 后）。
     *   真机反馈：起播后到歌词到达之间，多任务视图封面是一块黑，歌词一上来
     *   就恢复了 —— 说明只是"那一下"没重绘。补一次通知等价于人为制造一次
     *   内容变化，把这段窗口填上。
     */
    QTimer::singleShot(800, this, SLOT(notifyCoverAgain()));
    }
}

void MusicController::onPlaylistFinished(int requestId,
                                         const nm::NmParsers::PlaylistParse &result)
{
    Q_UNUSED(requestId);

    if (!result.ok) {
        /*
         * ★「全部歌曲」是多个歌单串行拉取的：单个歌单失败【不能】中断整轮 ——
         *   跳过它继续拉下一个，否则一个私密/异常歌单就会让整页空掉。
         */
        if (m_allSongsFetching) {
            if (!m_allSongsPendingIds.isEmpty()) {
                m_allSongsFetchId = m_allSongsPendingIds.takeFirst();
                m_api->fetchPlaylist(m_allSongsFetchId);
                return;
            }
            finishAllSongsFetch();
            return;
        }
        setError(result.error);
        finishRequest(QLatin1String("playlist"), false);
        return;
    }

    /*
     * ★「全部歌曲」页（照官方 Music ui 的 All Songs）走【独立模型】，
     *   绝不能灌进全局 songs，否则资料库/推荐的列表会被顶掉。
     *   这里必须放在 fillSongsModel 之前分流。
     */
    /*
     * ★ 必须确认"这个响应就是我要的那一个"（比对歌单 id）：
     *   「全部歌曲」是串行拉多个歌单，而用户随时可能中途打开别的歌单。
     *   那些响应不能被当成合并批次 —— 否则会被当普通歌单处理，
     *   还会把内容写进那个歌单的缓存（下次进去闪内容）。
     */
    const QString respId =
        result.info.toVariantMap().value(QLatin1String("id")).toString();
    if (m_allSongsFetching && ! respId.isEmpty() && respId == m_allSongsFetchId) {
        appendAllSongsBatch(result.songs);
        // 还有歌单没拉完 → 接着拉下一个；全部拉完才统一收尾（去重 + 重建）
        if (!m_allSongsPendingIds.isEmpty()) {
            m_allSongsFetchId = m_allSongsPendingIds.takeFirst();
            m_api->fetchPlaylist(m_allSongsFetchId);
            return;
        }
        finishAllSongsFetch();
        return;
    }

    /*
     * ★★ 顺序很重要：先设歌单信息，再填曲目。
     *   反过来（先 fillSongsModel）的话，QML 在曲目到达那一刻去读
     *   listTitle 拿到的还是【上一个歌单】的名字 —— 而 playlistInfo
     *   随后才更新，listTitle 的 NOTIFY 又是 songsChanged，不会再通知
     *   一次，标题就一直停在旧的了。
     */
    m_playlistInfo = result.info.toVariantMap();
    ++m_playlistInfoVersion;
    emit playlistInfoChanged();

    fillSongsModel(result.songs);
    // 保险起见再通知一次，确保标题绑定拿到的是新值
    emit songsChanged();

    if (result.songs.isEmpty())
        setError(NmTr("E6AD8CE58D95E4B8BAE7A9BAE68896E5B7B2E8A2ABE4B88BE69EB6"));

    /*
     * 存本地：下次点这个歌单先显示缓存（资料库高频访问）。
     * ★ 必须确认响应的 id == 当前打开的歌单 id，否则会把别的请求的结果写进
     *   这个歌单的缓存 —— 下次进去先显示错的缓存、网络回来再整体换掉（闪内容）。
     */
    if (!result.songs.isEmpty() && !m_pendingPlaylistId.isEmpty()
            && respId == m_pendingPlaylistId)
        savePlaylistSongsCache(m_pendingPlaylistId);

    finishRequest(QLatin1String("playlist"), true);
}

void MusicController::onSongUrlFinished(int requestId, qint64 songId,
                                        const nm::NmParsers::SongUrlParse &result)
{
    Q_UNUSED(requestId);
    Q_UNUSED(songId);

    if (!result.ok) {
        setError(result.error);
        finishRequest(QLatin1String("songUrl"), false);
        return;
    }

    if (!result.url.hasUrl()) {
        // code -110：未登录或无权限（VIP 曲目）
        QVariantMap detail;
        detail.insert(QLatin1String("code"), result.url.code);
        if (result.url.code == -110) {
            setError(NmTr("E697A0E6B395E692ADE694BEEFBC9AE69CAAE799BBE5BD95E68896564950E69BB2E79BAE"),
                     detail);
        } else {
            setError(NmTr("E697A0E6B395E692ADE694BEE6ADA4E69BB2E79BAE"), detail);
        }
        finishRequest(QLatin1String("songUrl"), false);
        return;
    }

    /*
     * ★★ 音频缓存（方案 A：先播、后台悄悄缓存）
     *   1) 本地已经有这首 → 直接用 file:// 播，一个字节都不耗；
     *   2) 没有 → 照常流式播（体验完全不变），同时让它在后台缓存下来，
     *      下次再听同一首就走本地文件了。
     */
    const QString cachedAudio = m_audioCache->pathFor(songId);
    if (! cachedAudio.isEmpty()) {
        m_playUrl = cachedAudio;
        qWarning("MusicController: audio cache HIT id=%lld", (long long)songId);
    } else {
        m_playUrl = result.url.url;
        m_audioCache->download(songId, result.url.url);
    }
    // 地址已经到手，去重的标记可以放开了（下一首/重播同一首会重新走一遍）
    if (m_pendingSongUrlId == result.url.id)
        m_pendingSongUrlId = 0;
    ++m_playUrlVersion;
    emit playUrlChanged();

    finishRequest(QLatin1String("songUrl"), true);
}

void MusicController::onAccountFinished(int requestId,
                                        const nm::NmParsers::AccountParse &result)
{
    Q_UNUSED(requestId);

    if (!result.ok || !result.account.valid) {
        m_loggedIn = false;
        m_nickName.clear();

        /*
         * ★ 关掉 loading：它是 login() / switchAccount() 开的，失败时
         *   后面不会再有请求来收尾，不关的话登录页会一直停在"正在校验登录态…"。
         */
        setLoading(false);

        if (!m_session->musicU().isEmpty()) {
            m_lastLoginError =
                NmTr("E799BBE5BD95E5B7B2E5A4B1E69588EFBC8CE8AFB7E9878DE696B0E7B298E8B4B44D555349435F55");
            emit lastLoginErrorChanged();
        }
        emit loginChanged();
        return;
    }

    m_loggedIn = true;
    m_nickName = result.account.nickname;
    m_userId = result.account.userId;
    m_vipType = result.account.vipType;
    if (!result.account.avatarUrl.isEmpty())
        m_userAvatarUrl = result.account.avatarUrl;
    emit profileChanged();

    /*
     * ★ 校验通过 = 这份凭证有效 → 记进"已保存账号"（多账号用）。
     *   id 用 userId，昵称/头像一并存下来 —— 以后切账号不联网也能显示名字。
     *   升级兼容：老数据里那条 id 为空的占位记录会在这里被认领并补齐。
     *（rememberCurrentAccount 会发 accountsChanged，模型自动跟着刷。）
     */
    m_session->rememberCurrentAccount(result.account.userId,
                                      result.account.nickname,
                                      result.account.avatarUrl);

    // 登录成功后顺带把「我的」页的数据都拉上（歌单 + 等级 + 关注艺人）
    if (m_userId > 0)
        m_api->fetchUserPlaylists(m_userId);
    m_api->fetchUserLevel();
    m_api->fetchSubscribedArtists(20, 0);
    if (!m_lastLoginError.isEmpty()) {
        m_lastLoginError.clear();
        emit lastLoginErrorChanged();
    }
    emit loginChanged();

    // 之前点了「我的歌单」但 uid 还没拿到：现在补拉
    if (m_pendingPlaylistsLoad) {
        m_pendingPlaylistsLoad = false;
        if (m_userId > 0)
            m_api->fetchUserPlaylists(m_userId);
        else
            finishRequest(QLatin1String("userPlaylists"), false);
    }
}

void MusicController::onDiscoverFinished(int requestId,
                                          const nm::NmParsers::DiscoverParse &result)
{
    Q_UNUSED(requestId);

    if (!result.ok) {
        setError(result.error);
        finishRequest(QLatin1String("discover"), false);
        return;
    }

    m_discoverPlaylists->clear();
    foreach (const nm::NmPlaylistSummary &pl, result.playlists) {
        QVariantMap map = pl.toVariantMap();
        map.insert(QLatin1String("localCoverPath"),
                   m_imageCache->pathFor(pl.coverUrl));
        map.insert(QLatin1String("type"), QLatin1String("playlist"));
        m_discoverPlaylists->append(map);
    }
    ++m_discoverVersion;
    emit discoverChanged();

    if (result.playlists.isEmpty())
        setError(NmTr("E68EA8E88D90E6AD8CE58D95E4B8BAE7A9BA"));   // 推荐歌单为空

    saveDiscoverCache();
    finishRequest(QLatin1String("discover"), true);
}

void MusicController::onRecommendFinished(int requestId,
                                          const nm::NmParsers::DiscoverParse &result)
{
    Q_UNUSED(requestId);

    if (!result.ok) {
        setError(result.error);
        m_recommendMode = false;
        finishRequest(QLatin1String("recommend"), false);
        return;
    }

    m_recommendMode = true;     // 填独立模型，不动全局 songs
    fillSongsModel(result.songs);

    // 每日推荐返回的是完整老格式（有 album.picUrl），一般不用补，
    // 但个别曲目可能没有，走同一套兜底
    if (requestArtEnrichmentIfNeeded())
        return;

    if (result.songs.isEmpty())
        setError(NmTr("E6AF8FE697A5E68EA8E88D90E4B8BAE7A9BA"
                      "EFBC8CE8AFB7E58588E799BBE5BD95"));         // 每日推荐为空，请先登录

    m_playlistInfo.clear();
    m_playlistInfo.insert(QLatin1String("name"), QLatin1String("每日推荐"));
    ++m_playlistInfoVersion;
    emit playlistInfoChanged();

    m_recommendMode = false;
    finishRequest(QLatin1String("recommend"), true);
}

void MusicController::onMvsFinished(int requestId,
                                    const nm::NmParsers::MvsParse &result)
{
    Q_UNUSED(requestId);

    if (!result.ok) {
        setError(result.error);
        finishRequest(QLatin1String("mvs"), false);
        return;
    }

    // 同 fillSongsModel：MV 列表自己的封面要优先
    m_imageCache->cancelQueued();

    m_mvs->clear();
    foreach (const nm::NmMv &mv, result.mvs) {
        QVariantMap map = mv.toVariantMap();
        map.insert(QLatin1String("localCoverPath"),
                   m_imageCache->pathFor(mv.coverUrl));
        map.insert(QLatin1String("type"), QLatin1String("mv"));
        m_mvs->append(map);
    }
    ++m_mvsVersion;
    emit mvsChanged();

    if (result.mvs.isEmpty())
        setError(NmTr("E6B2A1E69C89E68FA5E588B0MVE58897E8A1A8"));   // 没有获取到MV列表

    saveMvCache();
    finishRequest(QLatin1String("mvs"), true);
}

void MusicController::onMvUrlFinished(int requestId,
                                      const nm::NmParsers::MvUrlParse &result)
{
    Q_UNUSED(requestId);

    if (!result.ok) {
        setError(result.error);
        finishRequest(QLatin1String("mvUrl"), false);
        return;
    }

    m_mvUrl = result.url;
    ++m_mvUrlVersion;
    emit mvUrlChanged();

    /*
     * ★ 诊断：MV 播不出来时靠这条判断到底是"没取到地址"还是"地址取到了
     *   但播放器不认"。开「输出控制台日志」后能看到。
     */
    qWarning("MusicController: mvUrl id=%lld len=%d url=%s",
             (long long)result.id, m_mvUrl.size(),
             qPrintable(m_mvUrl.left(160)));

    finishRequest(QLatin1String("mvUrl"), true);
}

void MusicController::onLevelFinished(int requestId,
                                      const nm::NmParsers::LevelParse &result)
{
    Q_UNUSED(requestId);

    // 等级拉不到不影响别的，静默处理
    if (!result.ok)
        return;

    m_userLevel = result.level;
    m_userPlayCount = result.playCount;
    emit profileChanged();
}

void MusicController::onArtistsFinished(int requestId,
                                        const nm::NmParsers::ArtistsParse &result)
{
    Q_UNUSED(requestId);

    m_profileFetching = false;

    if (!result.ok)
        return;

    m_artists->clear();
    foreach (const nm::NmArtist &artist, result.artists) {
        QVariantMap map = artist.toVariantMap();
        map.insert(QLatin1String("localPicPath"),
                   m_imageCache->pathFor(artist.picUrl));
        map.insert(QLatin1String("type"), QLatin1String("artist"));
        m_artists->append(map);
    }
    ++m_artistsVersion;
    emit artistsChanged();

    saveArtistsCache();
}

void MusicController::onCommentsFinished(int requestId,
                                         const nm::NmParsers::CommentsParse &result)
{
    Q_UNUSED(requestId);

    if (!result.ok) {
        setError(result.error);
        finishRequest(QLatin1String("comments"), false);
        return;
    }

    fillCommentsModel(result.hotComments, result.comments);
    m_commentTotal = result.total;

    if (result.hotComments.isEmpty() && result.comments.isEmpty())
        setError(NmTr("E8BF99E9A696E6AD8CE8BF98E6B2A1E69C89E8AF84E8AEBA"));

    finishRequest(QLatin1String("comments"), true);
}

void MusicController::fillCommentsModel(const QList<nm::NmComment> &hot,
                                        const QList<nm::NmComment> &normal)
{
    m_comments->clear();

    // 热评排在前面，用 isHot 标记（列表项据此显示「热评」角标）
    foreach (const nm::NmComment &comment, hot) {
        QVariantMap map = comment.toVariantMap();
        map.insert(QLatin1String("localAvatarPath"),
                   m_imageCache->pathFor(comment.userAvatarUrl));
        m_comments->append(map);
    }
    foreach (const nm::NmComment &comment, normal) {
        QVariantMap map = comment.toVariantMap();
        map.insert(QLatin1String("localAvatarPath"),
                   m_imageCache->pathFor(comment.userAvatarUrl));
        m_comments->append(map);
    }

    ++m_commentsVersion;
    emit commentsChanged();
}

void MusicController::onUserPlaylistsFinished(
        int requestId, const nm::NmParsers::UserPlaylistsParse &result)
{
    Q_UNUSED(requestId);

    m_playlistsFetching = false;

    // 诊断：结果进的是"我的"还是"别人的"，以及条数
    qWarning("MusicController: onUserPlaylistsFinished ok=%d viewedMode=%d count=%d",
             result.ok ? 1 : 0, m_viewedUserMode ? 1 : 0, result.playlists.size());

    if (!result.ok) {
        // ★ 失败了也要把"看别人"的标记清掉，否则下一次拉「我的歌单」
        //   会被误判成别人的、塞进 viewedPlaylists（列表就空了）
        m_viewedUserMode = false;
        setError(result.error);
        finishRequest(QLatin1String("userPlaylists"), false);
        return;
    }

    /*
     * ★ 看【别人】的资料（m_viewedUserMode）时结果进 viewedPlaylists，
     *   而且【绝不动 m_favoritePlaylistId、也不写本地缓存】：
     *   那两个都是"我的" —— 被别人的数据覆盖的话，
     *   「全部歌曲」（要拉我喜欢的音乐）和下次启动的离线歌单列表就全错了。
     */
    if (m_viewedUserMode) {
        m_viewedUserMode = false;

        m_viewedPlaylists->clear();
        foreach (const nm::NmPlaylistSummary &pl, result.playlists) {
            QVariantMap map = pl.toVariantMap();
            map.insert(QLatin1String("localCoverPath"),
                       m_imageCache->pathFor(pl.coverUrl));
            map.insert(QLatin1String("type"), QLatin1String("playlist"));
            m_viewedPlaylists->append(map);
        }
        emit viewedPlaylistsChanged();

        finishRequest(QLatin1String("userPlaylists"), true);
        return;
    }

    m_playlists->clear();
    m_favoritePlaylistId.clear();
    foreach (const nm::NmPlaylistSummary &pl, result.playlists) {
        QVariantMap map = pl.toVariantMap();
        map.insert(QLatin1String("localCoverPath"),
                   m_imageCache->pathFor(pl.coverUrl));
        map.insert(QLatin1String("type"), QLatin1String("playlist"));
        m_playlists->append(map);

        // specialType==5 是「我喜欢的音乐」
        if (m_favoritePlaylistId.isEmpty() && pl.specialType == 5)
            m_favoritePlaylistId = pl.id;
    }
    ++m_playlistsVersion;
    emit playlistsChanged();

    // 资料库那份分组模型跟着重建（我创建的 / 我收藏的）
    rebuildLibraryPlaylists();

    // 存本地：下次启动能立刻显示（不用手点刷新）
    savePlaylistCache();

    if (result.playlists.isEmpty())
        setError(NmTr("E6B2A1E69C89E6898EE588B0E6AD8CE58D95"));

    finishRequest(QLatin1String("userPlaylists"), true);
}

void MusicController::onImageReady()
{
    ++m_imageCacheVersion;
    emit imageCacheChanged();
    emit coverInfoChanged();

    if (m_imageRefreshPending)
        return;                     // 已排程，多次完成合并成一次
    m_imageRefreshPending = true;

    // ★ 滚动中不刷新模型：边滚边 replace() 会让滑动明显卡顿。
    //   标记留着，滚动停止（setScrolling(false)）时立刻补一次。
    if (m_scrolling)
        return;

    m_imageRefreshTimer->start();
}

void MusicController::loadMyProfile()
{
    if (!m_session->hasCookie()) {
        setError(NmTr("E8AFB7E58588E799BBE5BD95"));   // 请先登录
        return;
    }

    // 去重：正在拉就不重复发
    if (m_profileFetching)
        return;
    m_profileFetching = true;

    setLoading(true);
    m_api->fetchUserLevel();
    m_api->fetchSubscribedArtists(20, 0);
}

void MusicController::playbackCompleted()
{
    if (songCount() <= 0)
        return;

    switch (m_repeatMode) {
    case 2: // 单曲循环：从头重播当前
        if (m_mediaPlayer) {
            m_mediaPlayer->seekTime(0);
            m_mediaPlayer->play();
        }
        break;
    case 1: // 列表循环（默认）
    default:
        next();
        break;
    }
}

void MusicController::loadMvList(const QString &kind)
{
    setLoading(true);
    m_api->fetchMvList(kind, 30, 0);
}

void MusicController::requestMvUrl(int index)
{
    if (index < 0 || index >= m_mvs->size()) {
        setError(NmTr("E697A0E69588E79A84E4B88BE6A087"));   // 无效的下标
        return;
    }

    const QVariantMap map = m_mvs->value(index).toMap();
    const qint64 mvId = map.value(QLatin1String("id")).toString().toLongLong();
    if (mvId <= 0) {
        setError(NmTr("E697A0E69588E79A84E4B88BE6A087"));
        return;
    }

    m_mvName = map.value(QLatin1String("name")).toString();
    m_mvArtist = map.value(QLatin1String("artistName")).toString();
    setLoading(true);
    m_api->fetchMvUrl(mvId, 480);
}

void MusicController::openMv(const QString &mvId, const QString &name,
                             const QString &artist)
{
    const qint64 id = mvId.toLongLong();
    if (id <= 0) {
        notifyError(QString::fromUtf8("该歌曲没有 MV"));
        return;
    }

    m_mvName = name;
    m_mvArtist = artist;
    m_currentMvId = id;     // ★ 同上：MV 页「评论」分段要用
    setLoading(true);
    // 复用 MV 播放地址接口；拿到后 mvUrlVersion 变化，main.qml 会推播放页
    m_api->fetchMvUrl(id, 480);
}

void MusicController::requestMvComments()
{
    if (m_currentMvId <= 0) {
        setError(NmTr("E697A0E69588E79A84E4B88BE6A087"));   // 无效的下标
        return;
    }

    // 换资源前先清，避免短暂显示上一个资源的评论
    m_commentsTitle = QString::fromUtf8("MV评论");
    clearComments();
    setLoading(true);
    m_api->fetchMvComments(m_currentMvId, 30, 0);
}

int MusicController::clearImageCache()
{
    return m_imageCache->clear();
}

void MusicController::clearPlaylistCache()
{
    QSettings settings;
    settings.remove(QLatin1String("nm/playlists"));
    settings.remove(QLatin1String("nm/favoriteId"));
    settings.remove(QLatin1String("nm/discover"));
    settings.remove(QLatin1String("nm/mvs"));
    settings.remove(QLatin1String("nm/artists"));

    /*
     * ★ 还要清【曲目缓存】。
     *   每个歌单的曲目存在 nm/plsongs/<id> 与 nm/plsongs_ids 下 ——
     *   之前这里漏了，于是"清除歌单缓存"点完，歌单列表是空的、
     *   点进去却还能看到旧曲目（真机反馈）。
     *   QSettings 没有通配符删除，只能把所有 key 列出来逐个删。
     */
    const QStringList keys = settings.allKeys();
    for (int i = 0; i < keys.size(); ++i) {
        if (keys.at(i).startsWith(QLatin1String("nm/plsongs")))
            settings.remove(keys.at(i));
    }
    // 「全部歌曲」的合并结果缓存也一并清掉
    settings.remove(QLatin1String("nm/allsongs"));
}

void MusicController::setAudioCacheLimit(int mb)
{
    m_audioCache->setLimitMb(mb);
}

int MusicController::audioCacheLimit() const
{
    return m_audioCache->limitMb();
}

int MusicController::clearAudioCache()
{
    return m_audioCache->clear();
}

QString MusicController::cacheInfo() const
{
    const qint64 bytes = m_imageCache->cacheSizeBytes();
    QStringList lines;
    lines << QString(QLatin1String("dir    : %1")).arg(m_imageCache->cacheDir());

    // 音频缓存（播放时后台存下来的歌曲文件）
    lines << QString(QLatin1String("audio  : %1 files / %2 MB"))
             .arg(m_audioCache->fileCount())
             .arg(QString::number(
                      m_audioCache->cacheSizeBytes() / (1024.0 * 1024.0), 'f', 1));
    // 文件数把两份缓存加起来（小图 + 播放页大图），否则会少报一半
    lines << QString(QLatin1String("cached : %1 files"))
                 .arg(m_imageCache->cachedCount()
                      + (m_bigImageCache ? m_bigImageCache->cachedCount() : 0));
    lines << QString(QLatin1String("size   : %1 KB"))
                 .arg(bytes / 1024);
    return lines.join(QLatin1String("\n"));
}

void MusicController::loadPlaylistCache()
{
    QSettings settings;
    const QVariantList list = settings.value(QLatin1String("nm/playlists")).toList();
    if (list.isEmpty())
        return;

    m_playlists->clear();
    foreach (const QVariant &item, list) {
        QVariantMap map = item.toMap();
        map.insert(QLatin1String("localCoverPath"),
                   m_imageCache->pathFor(map.value(QLatin1String("coverUrl")).toString()));
        m_playlists->append(map);
    }
    m_favoritePlaylistId = settings.value(QLatin1String("nm/favoriteId")).toString();
    ++m_playlistsVersion;
    emit playlistsChanged();

    // 资料库那份分组模型跟着重建（我创建的 / 我收藏的）
    rebuildLibraryPlaylists();
}

void MusicController::savePlaylistCache()
{
    QSettings settings;
    QVariantList list;
    for (int i = 0; i < m_playlists->size(); ++i)
        list.append(m_playlists->value(i).toMap());
    settings.setValue(QLatin1String("nm/playlists"), list);
    settings.setValue(QLatin1String("nm/favoriteId"), m_favoritePlaylistId);
}

void MusicController::loadDiscoverCache()
{
    QSettings settings;
    const QVariantList list = settings.value(QLatin1String("nm/discover")).toList();
    if (list.isEmpty())
        return;

    m_discoverPlaylists->clear();
    foreach (const QVariant &item, list) {
        QVariantMap map = item.toMap();
        map.insert(QLatin1String("localCoverPath"),
                   m_imageCache->pathFor(map.value(QLatin1String("coverUrl")).toString()));
        m_discoverPlaylists->append(map);
    }
    ++m_discoverVersion;
    emit discoverChanged();
}

void MusicController::saveDiscoverCache()
{
    QSettings settings;
    QVariantList list;
    for (int i = 0; i < m_discoverPlaylists->size(); ++i)
        list.append(m_discoverPlaylists->value(i).toMap());
    settings.setValue(QLatin1String("nm/discover"), list);
}

void MusicController::loadMvCache()
{
    QSettings settings;
    const QVariantList list = settings.value(QLatin1String("nm/mvs")).toList();
    if (list.isEmpty())
        return;

    m_mvs->clear();
    foreach (const QVariant &item, list) {
        QVariantMap map = item.toMap();
        map.insert(QLatin1String("localCoverPath"),
                   m_imageCache->pathFor(map.value(QLatin1String("coverUrl")).toString()));
        m_mvs->append(map);
    }
    ++m_mvsVersion;
    emit mvsChanged();
}

void MusicController::saveMvCache()
{
    QSettings settings;
    QVariantList list;
    for (int i = 0; i < m_mvs->size(); ++i)
        list.append(m_mvs->value(i).toMap());
    settings.setValue(QLatin1String("nm/mvs"), list);
}

void MusicController::loadArtistsCache()
{
    QSettings settings;
    const QVariantList list = settings.value(QLatin1String("nm/artists")).toList();
    if (list.isEmpty())
        return;

    m_artists->clear();
    foreach (const QVariant &item, list) {
        QVariantMap map = item.toMap();
        map.insert(QLatin1String("localPicPath"),
                   m_imageCache->pathFor(map.value(QLatin1String("picUrl")).toString()));
        m_artists->append(map);
    }
    ++m_artistsVersion;
    emit artistsChanged();
}

void MusicController::saveArtistsCache()
{
    QSettings settings;
    QVariantList list;
    for (int i = 0; i < m_artists->size(); ++i)
        list.append(m_artists->value(i).toMap());
    settings.setValue(QLatin1String("nm/artists"), list);
}

void MusicController::requestCommentsForIndex(int index)
{
    m_commentsAutoOpen = true;
    loadCommentsForIndex(index);
}

void MusicController::requestCommentsById(const QString &id)
{
    /*
     * 「全部歌曲」页专用：模型是 GroupDataModel，ListItem.indexPath 是
     * [组, 项] 两层，indexPath[0] 是组下标 —— 用 loadCommentsForIndex
     * 会评论到"那一组的第一首"，所以这里直接按歌曲 id 拉。
     */
    const qint64 songId = id.toLongLong();
    if (songId <= 0) {
        setError(NmTr("E697A0E69588E79A84E4B88BE6A087"));   // 无效的下标
        return;
    }

    m_commentsTitle = NmTr("E6AD8CE69BB2E8AF84E8AEBA");   // 歌曲评论

    m_commentsAutoOpen = true;
    clearComments();
    setLoading(true);
    m_api->fetchComments(songId, 30, 0);
}

void MusicController::requestPlaylistComments()
{
    /*
     * 歌单页的「评论」要的是【歌单自己】的评论（A_PL_0_<歌单 id>）。
     * 当前列表不是歌单（比如搜索结果）时 m_playlistInfo 里没有 id，
     * 这时退回看当前播放曲目的评论 —— 总比点了没反应强。
     */
    const qint64 playlistId =
        m_playlistInfo.value(QLatin1String("id")).toString().toLongLong();

    if (playlistId <= 0) {
        setError(NmTr("E8BF99E4B8AAE58897E8A1A8E4B88DE698AFE6AD8CE58D95EFBC8CE694B9E79C8BE5BD93E5898DE692ADE694BEE69BB2E79BAEE79A84E8AF84E8AEBA"));
        requestCommentsForCurrent();
        return;
    }

    m_commentsTitle = NmTr("E6AD8CE58D95E8AF84E8AEBA");   // 歌单评论

    m_commentsAutoOpen = true;
    clearComments();
    setLoading(true);
    m_api->fetchPlaylistComments(playlistId, 30, 0);
}

void MusicController::clearCommentsAutoOpen()
{
    /*
     * 这里【不能】emit commentsChanged()：commentsVersion 的 NOTIFY 就是它，
     * 在页面层的 onCommentsVersionChanged 里发这个信号会让绑定又求值一遍，
     * 触发 "Binding loop detected for property commentsVersion"。
     * 只是清个标志而已，不需要通知。
     */
    m_commentsAutoOpen = false;
}

void MusicController::setScrolling(bool scrolling)
{
    if (scrolling == m_scrolling)
        return;
    m_scrolling = scrolling;

    // 刚滚完：把攒下的刷新补上
    if (!m_scrolling && m_imageRefreshPending)
        m_imageRefreshTimer->start();
}

/*!
 * 把 GroupDataModel 里各条的 localKey 刷成最新下好的本地路径。
 * ★ 先收集、再统一 updateItem：边遍历边改模型容易踩到顺序变化。
 */
static void nmRefreshGroupImages(nm::NmImageCache *cache,
                                 bb::cascades::GroupDataModel *model,
                                 const char *remoteKey, const char *localKey)
{
    if (!cache || !model)
        return;

    const QString remoteK = QLatin1String(remoteKey);
    const QString localK = QLatin1String(localKey);

    QList<QPair<QVariantList, QVariantMap> > updates;
    for (QVariantList ip = model->first(); !ip.isEmpty(); ip = model->after(ip)) {
        const QVariantMap map = model->data(ip).toMap();
        const QString remote = map.value(remoteK).toString();
        if (remote.isEmpty())
            continue;
        const QString fresh = cache->pathFor(remote);
        if (fresh.isEmpty() || fresh == map.value(localK).toString())
            continue;
        QVariantMap updated = map;
        updated.insert(localK, fresh);
        updates.append(qMakePair(ip, updated));
    }
    for (int i = 0; i < updates.size(); ++i)
        model->updateItem(updates.at(i).first, updates.at(i).second);
}

void MusicController::refreshImagePaths()
{
    m_imageRefreshPending = false;

    struct Target { ArrayDataModel *model; const char *remoteKey; const char *localKey; };
    const Target targets[] = {
        { m_songs,             "artUrl",        "localArtPath"    },
        { m_playlists,         "coverUrl",      "localCoverPath"  },
        { m_discoverPlaylists, "coverUrl",      "localCoverPath"  },
        { m_comments,          "userAvatarUrl", "localAvatarPath" },
        { m_artists,           "picUrl",        "localPicPath"    },
        { m_mvs,               "coverUrl",      "localCoverPath"  },
        { m_albums,            "artUrl",        "localCoverPath"  },
        // 后加的两个：不列在这里的话，图下完了模型里那条 localXxx 也永远是空
        { m_otherAccounts,     "avatarUrl",     "localAvatarPath" },
        /*
         * ★★ 后加的这几个模型也必须列进来，否则封面/头像下完了
         *   它们那条 localXxx 永远是空 —— 表现就是"列表里封面加载不出来"：
         *     m_browseSongs    歌单 / 专辑 / 搜索结果页（之前一直没图）
         *     m_artistSongs    艺人页「热门」（有的有图有的没有）
         *     m_recommendSongs 每日推荐
         */
        { m_browseSongs,       "artUrl",        "localArtPath"    },
        { m_artistSongs,       "artUrl",        "localArtPath"    },
        { m_recommendSongs,    "artUrl",        "localArtPath"    }
    };
    const int targetCount = (int)(sizeof(targets) / sizeof(targets[0]));

    for (int t = 0; t < targetCount; ++t) {
        ArrayDataModel *model = targets[t].model;
        if (!model)
            continue;
        const QString remoteKey = QLatin1String(targets[t].remoteKey);
        const QString localKey = QLatin1String(targets[t].localKey);

        /*
         * 每次都重新读 size() 并在循环里逐次校验（延迟执行期间
         * 模型可能被翻页/重查改变长度，越界 replace 是内存破坏）。
         */
        for (int i = 0; i < model->size(); ++i) {
            const QVariantMap map = model->value(i).toMap();
            const QString remote = map.value(remoteKey).toString();
            if (remote.isEmpty())
                continue;

            const QString fresh = m_imageCache->pathFor(remote);
            if (fresh.isEmpty())
                continue;                       // 还没下完，保持原样
            if (fresh == map.value(localKey).toString())
                continue;                       // 没变化，不用动

            QVariantMap updated = map;
            updated.insert(localKey, fresh);
            if (i < model->size())
                model->replace(i, updated);
        }
    }

    /*
     * 资料库那两个 GroupDataModel（艺术家 / 歌单）类型对不上上面那张
     * ArrayDataModel 表，单独刷一遍 —— 否则图下完了模型里那条
     * localCoverPath 也永远是空。
     */
    nmRefreshGroupImages(m_imageCache, m_libraryArtists, "artUrl", "localCoverPath");
    nmRefreshGroupImages(m_imageCache, m_libraryPlaylists, "coverUrl", "localCoverPath");
}

void MusicController::onRequestFailed(int requestId, const QString &tag,
                                      const QString &message,
                                      const QVariantMap &detail)
{
    Q_UNUSED(requestId);
    setError(message, detail);
    // 请求失败也要把去重标志清掉，否则之后再点就永远被挡
    if (tag == QLatin1String("userPlaylists"))
        m_playlistsFetching = false;
    else if (tag == QLatin1String("artists"))
        m_profileFetching = false;
    finishRequest(tag, true /* 视为该业务已结束（失败也是结束） */);
}
