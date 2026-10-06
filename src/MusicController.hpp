/*
 * MusicController - 逻辑层与 QML 之间的唯一接口（含播放队列）
 *
 * 设计约定（与 BBTieba 相同，换 UI 时请遵守）：
 *   - QML 只通过这个对象干活，不直接碰网络和解析；
 *   - 列表是 bb::cascades::ArrayDataModel，列表项是 QVariantMap
 *     （QML 里 ListItemData.<key> 直接取值）；
 *   - 取数据都是异步的：调 loadXxx() 后监听 xxxChanged 信号或用
 *     loading / errorMessage 属性驱动界面。
 *
 * 播放的控制面在 QML 侧的 MediaPlayer（bb.multimedia）：
 *   - QML 里绑一个页面级属性到 music.playUrlVersion / music.playUrl，
 *     变化时设置 player.sourceUrl 并 play()；
 *   - 自动切歌：MediaPlayer 的 onPlaybackCompleted 调 music.next()
 *     （playing/paused/stopped 信号不存在，勿用）。
 *   （Cascades QML 监听不到 context property 的信号，所以用
 *    「属性 + 版本号」的绑定模式触发，见 TiebaController 的同款说明。）
 *
 * 暴露给 QML 的属性：
 *   songs        当前歌曲列表     （ArrayDataModel，项字段见 NmSong::toVariantMap）
 *   playlistInfo 当前歌单信息     （QVariantMap + playlistInfoVersion）
 *   currentTrack 正在播放的曲目   （QVariantMap + currentTrackVersion）
 *   currentIndex 当前曲目下标     （-1 = 无）
 *   playUrl      当前播放地址     （QString + playUrlVersion，空 = 不可播）
 *   loading / errorMessage / errorDetail / lastRaw
 *   loggedIn / nickName / lastLoginError
 *   bitrate      期望码率
 */

#ifndef MUSIC_CONTROLLER_HPP
#define MUSIC_CONTROLLER_HPP

#include <QObject>
#include <QString>
#include <QUrl>          // coverImagePath 是 QUrl 属性，moc 需要完整类型
#include <QVariantList>
#include <QVariantMap>

#include "api/NmApi.hpp"
#include "model/NmItems.hpp"
#include "model/NmParsers.hpp"

namespace bb {
namespace cascades {
class ArrayDataModel;
class GroupDataModel;
}
namespace multimedia {
class MediaPlayer;
}
namespace system {
class SystemToast;
}
}

class QTimer;

namespace nm {
class NmImageCache;
class NmAudioCache;
}

class MusicController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bb::cascades::ArrayDataModel* songs READ songs NOTIFY songsChanged)

    /*! 我的歌单列表（项字段见 NmPlaylistSummary::toVariantMap + localCoverPath） */
    Q_PROPERTY(bb::cascades::ArrayDataModel* playlists READ playlists NOTIFY playlistsChanged)
    Q_PROPERTY(int playlistsVersion READ playlistsVersion NOTIFY playlistsChanged)

    /*!
     * 【资料库 → 播放列表】用的分组版：同一个数据，按 subscribed 分成
     * 「我创建的 / 我收藏的」两组（GroupDataModel 所以有置顶分组条）。
     * ★ 单独一个模型而不是把 playlists 换掉：那个还被用户 Tab / 个人主页
     *   用着，换类型的话那两处的委托也得跟着改成 header/item 两套。
     */
    Q_PROPERTY(bb::cascades::GroupDataModel* libraryPlaylists READ libraryPlaylists
               NOTIFY libraryPlaylistsChanged)
    Q_PROPERTY(int libraryPlaylistsVersion READ libraryPlaylistsVersion
               NOTIFY libraryPlaylistsChanged)

    /*!
     * 正在查看的【别人】的资料（评论区长按 →「打开用户资料」）。
     * 项字段：userId / nickname / avatarUrl。
     * ★ 昵称和头像直接由评论带过来，页面一推出来就能显示，不用等接口。
     */
    Q_PROPERTY(QVariantMap viewedUser READ viewedUser NOTIFY viewedUserChanged)

    /*!
     * 正在查看的那个人的歌单。
     * ★ 单独一个模型，【不要复用 playlists】—— 那个是"我的歌单"，
     *   用户 Tab / 资料库都在用，顶掉的话要重新拉一遍才能恢复。
     */
    Q_PROPERTY(bb::cascades::ArrayDataModel* viewedPlaylists READ viewedPlaylists
               NOTIFY viewedPlaylistsChanged)

    /*!
     * 当前账号（账号管理页的「当前账号」一段）。
     * 字段：id / nickName / userId / avatarUrl / masked / isCurrent；未登录时是空 map。
     */
    Q_PROPERTY(QVariantMap currentAccount READ currentAccount NOTIFY accountsChanged)

    /*! 其它已保存的账号（账号管理页的「已登录」一段） */
    Q_PROPERTY(bb::cascades::ArrayDataModel* otherAccounts READ otherAccounts
               NOTIFY accountsChanged)
    Q_PROPERTY(int accountsVersion READ accountsVersion NOTIFY accountsChanged)
    /*! 已保存的其它账号条数（空状态提示用；ArrayDataModel.size() 在 QML 里不保险） */
    Q_PROPERTY(int otherAccountCount READ otherAccountCount NOTIFY accountsChanged)

    /*! 推荐歌单（同一个数据模型类型，走 discoverPlaylists / discoverVersion） */
    Q_PROPERTY(bb::cascades::ArrayDataModel* discoverPlaylists READ discoverPlaylists
               NOTIFY discoverChanged)
    Q_PROPERTY(int discoverVersion READ discoverVersion NOTIFY discoverChanged)

    /*! 当前歌曲的评论（热评在前） */
    Q_PROPERTY(bb::cascades::ArrayDataModel* comments READ comments NOTIFY commentsChanged)
    Q_PROPERTY(int commentsVersion READ commentsVersion NOTIFY commentsChanged)
    Q_PROPERTY(int commentTotal READ commentTotal NOTIFY commentsChanged)

    /*!
     * 评论页标题：歌曲评论 / 歌单评论（QML 的评论页标题栏用）。
     * NOTIFY 用 commentsChanged —— 它和内容同时更新，避免多一个信号。
     */
    /*!
     * 歌词开关（设置页「内容」区）：为真时【主界面】迷你条的标题显示
     * 当前这句歌词，副标题显示「歌名 - 作者」。
     *
     * ★ 只作用于主界面那几条 NowPlayingBar，【不影响正在播放全屏页】——
     *   那里有自己的标题栏，两边互不干涉（解耦）。
     */
    Q_PROPERTY(bool lyricsEnabled READ lyricsEnabled WRITE setLyricsEnabled
               NOTIFY lyricsEnabledChanged)
    Q_PROPERTY(QString currentLyricLine READ currentLyricLine
               NOTIFY currentLyricLineChanged)

    /*!
     * 翻译开关：歌词模式下，为真时副标题显示【这句歌词的翻译】，
     * 为假时副标题显示「歌名 - 作者」。标题始终是原词（歌曲主语言）。
     */
    Q_PROPERTY(bool lyricsTransEnabled READ lyricsTransEnabled
               WRITE setLyricsTransEnabled NOTIFY lyricsTransEnabledChanged)
    Q_PROPERTY(QString currentLyricTrans READ currentLyricTrans
               NOTIFY currentLyricTransChanged)

    /*! 歌词开关（设置页「内容」区调 setLyricsEnabled） */
    bool lyricsEnabled() const;
    Q_INVOKABLE void setLyricsEnabled(bool on);
    /*! 当前这句歌词（随播放进度推进，见 onPlayerPositionChanged） */
    QString currentLyricLine() const;
    /*! 歌词翻译开关 */
    bool lyricsTransEnabled() const;
    Q_INVOKABLE void setLyricsTransEnabled(bool on);
    /*! 当前这句歌词的翻译（没有翻译时为空串） */
    QString currentLyricTrans() const;
    Q_PROPERTY(QString commentsTitle READ commentsTitle NOTIFY commentsChanged)
    QString commentsTitle() const { return m_commentsTitle; }

    Q_PROPERTY(QVariantMap playlistInfo READ playlistInfo NOTIFY playlistInfoChanged)
    Q_PROPERTY(int playlistInfoVersion READ playlistInfoVersion NOTIFY playlistInfoChanged)

    Q_PROPERTY(QVariantMap currentTrack READ currentTrack NOTIFY currentTrackChanged)
    Q_PROPERTY(int currentTrackVersion READ currentTrackVersion NOTIFY currentTrackChanged)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentTrackChanged)

    /*! 播放地址 + 版本号。QML 里：property string playUrl: music.playUrlVersion ? music.playUrl : "" */
    Q_PROPERTY(QString playUrl READ playUrl NOTIFY playUrlChanged)
    Q_PROPERTY(int playUrlVersion READ playUrlVersion NOTIFY playUrlChanged)

    /*! 播放进度（毫秒）。C++ 侧用 QTimer 每 250ms 推一次，供进度条跟随 */
    Q_PROPERTY(int playerPosition READ playerPosition NOTIFY playerPositionChanged)

    /*! 循环模式：1=列表循环 2=单曲循环（默认 1） */
    Q_PROPERTY(int repeatMode READ repeatMode NOTIFY repeatModeChanged)

    /*! 随机播放开关（和循环模式各自独立，互不影响） */
    Q_PROPERTY(bool shuffle READ shuffle NOTIFY shuffleChanged)

    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(QVariantMap errorDetail READ errorDetail NOTIFY errorDetailChanged)
    Q_PROPERTY(QString lastRaw READ lastRaw NOTIFY lastRawChanged)

    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY loginChanged)
    Q_PROPERTY(QString nickName READ nickName NOTIFY loginChanged)
    Q_PROPERTY(QString lastLoginError READ lastLoginError NOTIFY lastLoginErrorChanged)

    /*! 我的页：头像地址 / 等级 / 听歌数 / VIP 类型 */
    Q_PROPERTY(QString userAvatarUrl READ userAvatarUrl NOTIFY profileChanged)
    Q_PROPERTY(int userLevel READ userLevel NOTIFY profileChanged)
    Q_PROPERTY(int userPlayCount READ userPlayCount NOTIFY profileChanged)
    Q_PROPERTY(int vipType READ vipType NOTIFY profileChanged)

    /*! 我关注的艺人（项字段见 NmArtist::toVariantMap + localPicPath） */
    Q_PROPERTY(bb::cascades::ArrayDataModel* artists READ artists NOTIFY artistsChanged)
    Q_PROPERTY(int artistsVersion READ artistsVersion NOTIFY artistsChanged)

    /*!
     * 艺人资料页的【独立】歌曲/专辑模型。
     * ★ 不能让艺人页复用全局 songs/albums：那样一进艺人页，推荐页的
     *   「歌曲」就变成这个艺人的歌了（真机反馈）。所以走独立模型。
     */
    Q_PROPERTY(bb::cascades::ArrayDataModel* artistSongs READ artistSongs
               NOTIFY artistSongsChanged)
    Q_PROPERTY(int artistSongsVersion READ artistSongsVersion NOTIFY artistSongsChanged)
    Q_PROPERTY(bb::cascades::ArrayDataModel* artistAlbums READ artistAlbums
               NOTIFY artistSongsChanged)
    bb::cascades::ArrayDataModel *artistSongs() const { return m_artistSongs; }
    int artistSongsVersion() const { return m_artistSongsVersion; }
    bb::cascades::ArrayDataModel *artistAlbums() const { return m_artistAlbums; }

    /*!
     * 推荐页「歌曲」的【独立】模型（每日推荐）。
     * ★ 不能让推荐页绑定全局 songs：任何搜索（比如点艺人）都会替换全局
     *   songs，推荐页的「歌曲」就变成那个艺人的歌了（真机反馈的"污染"）。
     */
    Q_PROPERTY(bb::cascades::ArrayDataModel* recommendSongs READ recommendSongs
               NOTIFY recommendSongsChanged)
    Q_PROPERTY(int recommendSongsVersion READ recommendSongsVersion
               NOTIFY recommendSongsChanged)
    bb::cascades::ArrayDataModel *recommendSongs() const { return m_recommendSongs; }
    int recommendSongsVersion() const { return m_recommendSongsVersion; }

    /*!
     * 搜索页的【独立】歌曲模型。
     * ★ 不能让搜索页绑定全局 songs：播放任意一首歌都会用
     *   setPlaylistFrom() 把全局 songs 换成"那首歌所在的歌单/专辑"，
     *   搜索页一进去就显示成"当前歌相关的列表"（真机反馈）。
     */
    Q_PROPERTY(bb::cascades::ArrayDataModel* searchSongs READ searchSongs
               NOTIFY searchSongsChanged)
    Q_PROPERTY(int searchSongsVersion READ searchSongsVersion
               NOTIFY searchSongsChanged)
    bb::cascades::ArrayDataModel *searchSongs() const { return m_searchSongs; }
    int searchSongsVersion() const { return m_searchSongsVersion; }

    /*!
     * 专辑列表（项字段：id / name / artist / artUrl / localCoverPath / trackCount）
     *
     * 数据来源：把当前 songs 列表按 albumId 本地聚合。
     * 为什么不去查接口：网易这套老接口里没有"我的专辑"列表，而歌单/搜索
     * 结果本身已经带专辑信息，本地聚合零请求、零风险。
     */
    Q_PROPERTY(bb::cascades::ArrayDataModel* albums READ albums NOTIFY albumsChanged)
    Q_PROPERTY(int albumsVersion READ albumsVersion NOTIFY albumsChanged)

    /*! MV 列表（项字段见 NmMv::toVariantMap + localCoverPath） */
    Q_PROPERTY(bb::cascades::ArrayDataModel* mvs READ mvs NOTIFY mvsChanged)
    Q_PROPERTY(int mvsVersion READ mvsVersion NOTIFY mvsChanged)

    /*!
     * 「全部歌曲」=「我喜欢的音乐」歌单的曲目（独立模型）。
     * ★ 用 GroupDataModel：它天生是 root → 分组 header → 组内条目 的树状结构，
     *   配 ListHeaderMode::StickyOverlay 才有"置顶并被下一个顶掉"的效果
     *   （平铺 ArrayDataModel 做不到，见 stacklistlayout.h 原文说明）。
     */
    Q_PROPERTY(bb::cascades::GroupDataModel* allSongs READ allSongs NOTIFY allSongsChanged)
    Q_PROPERTY(int allSongsVersion READ allSongsVersion NOTIFY allSongsChanged)

    /*!
     * 浏览列表（歌单 / 专辑 / 搜索结果「只是打开来看」时用）。
     * ★ 和 songs（播放队列）分开：浏览不会动队列，见 playBrowseSong。
     */
    Q_PROPERTY(bb::cascades::ArrayDataModel* browseSongs READ browseSongs
               NOTIFY browseSongsChanged)
    Q_PROPERTY(int browseSongsVersion READ browseSongsVersion NOTIFY browseSongsChanged)
    /*! 浏览列表曲目数（同 browseCount()，多了 NOTIFY，供绑定用） */
    Q_PROPERTY(int browseTotal READ browseTotal NOTIFY browseSongsChanged)

    /*!
     * 【资料库 → 艺术家】按艺人归类的曲目索引（像 Apple Music 的艺术家页）。
     *
     * ★ 数据来自「全部歌曲」那份本地列表（m_allSongMaps），不额外发请求 ——
     *   就是把手上已有的歌按 artistsText 分组。字段：
     *     name / songCount / artUrl / localCoverPath
     * ★ 所以这里【不需要】"关注的艺人"接口（那个只对登录用户有效，
     *   见 tools/probe-user-artists.ps1 的实测）。
     */
    Q_PROPERTY(bb::cascades::GroupDataModel* libraryArtists READ libraryArtists
               NOTIFY libraryArtistsChanged)
    Q_PROPERTY(int libraryArtistsVersion READ libraryArtistsVersion
               NOTIFY libraryArtistsChanged)
    /*! 归出来的艺人数（空状态提示用） */
    Q_PROPERTY(int libraryArtistCount READ libraryArtistCount
               NOTIFY libraryArtistsChanged)

    /*! 当前选中 MV 的播放地址（带时效性签名，拿到后尽快用） */
    Q_PROPERTY(QString mvUrl READ mvUrl NOTIFY mvUrlChanged)
    Q_PROPERTY(int mvUrlVersion READ mvUrlVersion NOTIFY mvUrlChanged)
    Q_PROPERTY(QString mvName READ mvName NOTIFY mvUrlChanged)
    Q_PROPERTY(QString mvArtist READ mvArtist NOTIFY mvUrlChanged)

    Q_PROPERTY(int bitrate READ bitrate WRITE setBitrate NOTIFY bitrateChanged)

    /*!
     * 主题："" = 跟随系统（bar-descriptor 的 CASCADES_THEME=default），
     * 或 "Bright" / "Dark"。设置页的暗色开关用它；切换立即生效并持久化。
     */
    Q_PROPERTY(QString theme READ theme NOTIFY themeChanged)

    /*! 隐藏 VIP 歌曲（设置页开关）。开启后各处歌曲列表不显示 VIP 曲目 */
    Q_PROPERTY(bool hideVip READ hideVip WRITE setHideVip NOTIFY hideVipChanged)

    /*!
     * 独立加载的页面（拿不到 main.qml 的 root）想「打开曲目页」时，
     * 通过这里发请求，main.qml 监听版本号后去推页。
     *
     * ★ 为什么不用「给页面塞一个 JS 回调属性」：那样 QML 会把赋进去的
     *   函数当成绑定，触发 "Binding loop detected"，循环抖动还会让
     *   ListView.onTriggered 反复触发 —— 表现为无限推页直到卡死。
     */
    Q_PROPERTY(int openRequestVersion READ openRequestVersion NOTIFY openRequestChanged)
    Q_PROPERTY(QString openRequestKind READ openRequestKind NOTIFY openRequestChanged)
    Q_PROPERTY(QString openRequestArg READ openRequestArg NOTIFY openRequestChanged)

    /*!
     * 图片缓存版本号。QML 的绑定里引用它，封面下载完成后
     * 绑定会被重新求值（列表项走模型字段不走这里，这里服务
     * 「正在播放」大封面那类页面级绑定）。
     */
    Q_PROPERTY(int imageCacheVersion READ imageCacheVersion NOTIFY imageCacheChanged)
    int imageCacheVersion() const { return m_imageCacheVersion; }

public:
    explicit MusicController(QObject *parent = 0);
    virtual ~MusicController();

    bb::cascades::ArrayDataModel *songs() const;

    bb::cascades::ArrayDataModel *playlists() const { return m_playlists; }
    int playlistsVersion() const { return m_playlistsVersion; }

    bb::cascades::GroupDataModel *libraryPlaylists() const { return m_libraryPlaylists; }
    int libraryPlaylistsVersion() const { return m_libraryPlaylistsVersion; }

    QVariantMap viewedUser() const { return m_viewedUser; }
    bb::cascades::ArrayDataModel *viewedPlaylists() const { return m_viewedPlaylists; }

    QVariantMap currentAccount() const;
    bb::cascades::ArrayDataModel *otherAccounts() const { return m_otherAccounts; }
    int accountsVersion() const { return m_accountsVersion; }
    int otherAccountCount() const;

    bb::cascades::ArrayDataModel *discoverPlaylists() const { return m_discoverPlaylists; }
    int discoverVersion() const { return m_discoverVersion; }

    bb::cascades::ArrayDataModel *comments() const { return m_comments; }
    int commentsVersion() const { return m_commentsVersion; }
    int commentTotal() const { return m_commentTotal; }
    /*! 已加载的评论条数。评论页空状态判断用它（比 commentTotal 可靠：
     *  某些歌曲 commentTotal==0 但热评仍在） */
    Q_INVOKABLE int commentCount() const;

    QVariantMap playlistInfo() const { return m_playlistInfo; }
    int playlistInfoVersion() const { return m_playlistInfoVersion; }

    QVariantMap currentTrack() const { return m_currentTrack; }
    int currentTrackVersion() const { return m_currentTrackVersion; }
    int currentIndex() const { return m_currentIndex; }

    QString playUrl() const { return m_playUrl; }
    int playUrlVersion() const { return m_playUrlVersion; }

    bool loading() const { return m_loading; }
    QString errorMessage() const { return m_errorMessage; }
    QVariantMap errorDetail() const { return m_errorDetail; }
    QString lastRaw() const { return m_lastRaw; }

    bool loggedIn() const { return m_loggedIn; }
    QString nickName() const { return m_nickName; }
    QString lastLoginError() const { return m_lastLoginError; }

    QString userAvatarUrl() const { return m_userAvatarUrl; }
    int userLevel() const { return m_userLevel; }
    int userPlayCount() const { return m_userPlayCount; }
    int vipType() const { return m_vipType; }

    bb::cascades::ArrayDataModel *artists() const { return m_artists; }
    int artistsVersion() const { return m_artistsVersion; }

    bb::cascades::ArrayDataModel *albums() const { return m_albums; }
    int albumsVersion() const { return m_albumsVersion; }

    bb::cascades::ArrayDataModel *mvs() const { return m_mvs; }
    int mvsVersion() const { return m_mvsVersion; }

    bb::cascades::GroupDataModel *allSongs() const { return m_allSongs; }
    int allSongsVersion() const { return m_allSongsVersion; }

    bb::cascades::ArrayDataModel *browseSongs() const { return m_browseSongs; }
    int browseSongsVersion() const { return m_browseSongsVersion; }

    /*! 浏览列表的曲目数（歌单页空状态 / 「播放」动作用；带 NOTIFY 的见 browseTotal） */
    Q_INVOKABLE int browseCount() const;
    int browseTotal() const;

    bb::cascades::GroupDataModel *libraryArtists() const { return m_libraryArtists; }
    int libraryArtistsVersion() const { return m_libraryArtistsVersion; }
    int libraryArtistCount() const;

    QString mvUrl() const { return m_mvUrl; }
    int mvUrlVersion() const { return m_mvUrlVersion; }
    QString mvName() const { return m_mvName; }
    QString mvArtist() const { return m_mvArtist; }

    int bitrate() const;

    /*! 当前主题：""=跟随系统 / "Bright" / "Dark" */
    QString theme() const;

    /*! 是否隐藏 VIP 歌曲（见 hideVip 属性） */
    bool hideVip() const;

    int openRequestVersion() const { return m_openRequestVersion; }
    QString openRequestKind() const { return m_openRequestKind; }
    QString openRequestArg() const { return m_openRequestArg; }

    /*!
     * 切换主题（"Bright" / "Dark"）：立即生效 + 写 QSettings 持久化。
     * ★ 必须是 Q_INVOKABLE，否则 QML 里报 "music.setTheme is not a function"。
     */
    Q_INVOKABLE void setTheme(const QString &theme);

    /*! 独立页面请求打开「曲目页」：arg 是歌单 id（kind="playlist"） */
    Q_INVOKABLE void requestOpenPlaylist(const QString &id);
    /*! 独立页面请求「搜索并打开曲目页」：arg 是关键词（kind="search"） */
    Q_INVOKABLE void requestOpenSearch(const QString &name);
    /*! 独立页面请求打开「艺人资料页」：arg 是艺人名（kind="artist"） */
    Q_INVOKABLE void requestOpenArtist(const QString &name);
    /*! 独立页面请求打开「专辑页」：arg 是专辑 id（kind="album"） */
    Q_INVOKABLE void requestOpenAlbum(const QString &albumId, const QString &name);
    /*! 按专辑 id 加载曲目（进全局 songs；listTitle 显示专辑名） */
    Q_INVOKABLE void loadAlbum(const QString &albumId);

    /*!
     * 把整张专辑的曲目【追加进播放队列】（艺人页专辑长按「增加到队列」）。
     *
     * ★ 和 loadAlbum() 的区别：loadAlbum 会用这张专辑【顶掉】当前列表
     *   （用来"打开专辑页"），这里只往队列里追加，不动当前正在看/播的内容。
     */
    Q_INVOKABLE void enqueueAlbum(const QString &albumId);
    /*! 按名字在"关注的艺人"里查资料（picUrl/albumSize/mvSize），查不到返回空 map */
    Q_INVOKABLE QVariantMap artistInfo(const QString &name) const;
    /*! main.qml 处理完请求后清掉，避免重复触发 */
    Q_INVOKABLE void clearOpenRequest();

    /*!
     * 启动时调用（applicationui 里、创建 UI 之前）：读 QSettings 里存的
     * 主题并应用；"跟随系统"时不动（交给 bar-descriptor 的 default）。
     */
    void applySavedTheme();

    /*!
     * applicationui 把全局播放器挂进来。
     * ★ 为什么要桥接：bb::multimedia::MediaPlayer 的 seekTime() 不是
     *   Q_INVOKABLE，QML 里调不到；时间条拖动松手要靠这里转一下。
     */
    void attachPlayer(bb::multimedia::MediaPlayer *player);
    /*! 跳到指定毫秒（时间条拖动松手时调用） */
    Q_INVOKABLE void seekTo(int msec);
    /*!
     * 拖动进度条期间暂停位置更新（照官方 TimeSlider 的 setSamplingMode）。
     * 不暂停的话，位置更新会把滑块拽回播放位置 —— 表现为「回弹」。
     */
    Q_INVOKABLE void setSeeking(bool seeking);
    /*! 当前播放进度（毫秒），由 m_positionTimer 轮询 MediaPlayer 得到 */
    int playerPosition() const;

    /*! 循环模式：1=列表循环 2=单曲循环（默认 1） */
    int repeatMode() const;
    void setRepeatMode(int m);
    Q_INVOKABLE void toggleRepeatMode();

    /*! 随机播放开关（独立于循环模式） */
    bool shuffle() const;
    void setShuffle(bool on);
    Q_INVOKABLE void toggleShuffle();

    /*! 资料库「最近播放」：用本地播放记录填 songs，标题设为"最近播放" */
    Q_INVOKABLE void loadRecentPlayed();

    /*! 「全部歌曲」：加载「我喜欢的音乐」歌单（缓存优先，联网刷新） */
    Q_INVOKABLE void loadAllSongs();

    /*!
     * 全部歌曲页的【页内搜索】：只在这份列表里按关键词过滤
     * （匹配歌名 / 艺人 / 专辑名），不影响全局 songs。
     */
    Q_INVOKABLE void setAllSongsFilter(const QString &text);

    /*!
     * 随机播放【全部歌曲】：把这份列表洗牌后当播放队列起播。
     * ★ 只在这份列表里随机，和全局 songs 无关。
     */
    Q_INVOKABLE void playAllSongsShuffled();

    /*!
     * 【资料库 → 艺术家】点某个艺人：把他名下的歌做成一个列表（复用播放列表页）。
     * 数据同样从「全部歌曲」里筛，不联网。
     */
    Q_INVOKABLE void loadLibraryArtist(const QString &name);
    /*!
     * 「全部歌曲」播放某首歌。
     * ★ 模型是 GroupDataModel，ListItem 的 indexPath 是 [组, 项] 两层，没法再按下标播；
     *   统一按歌曲 id 定位（id 唯一）。
     */
    Q_INVOKABLE void playAllSongById(const QString &id);
    /*!
     * 个人主页的背景图：没有"用户头图"接口，就从缓存里挑一张
     * 专辑/歌单封面凑合用（空串 = 一张都没有）。
     */
    Q_INVOKABLE QString bannerPath() const;

    /*! 关注的艺人数（ArtistsPage 的空状态判断用） */
    Q_INVOKABLE int artistCount() const;

    /*!
     * 列表快照 / 还原。
     * ★ 艺人资料页要 search 拿这个艺人的歌，而 search 会替换全局
     *   songs / albums；离开时不还原的话，推荐页的「歌曲」就变成这个
     *   艺人的歌了（真机反馈）。进艺人页前 snapshot，pop 时 restore。
     */
    Q_INVOKABLE void snapshotList();
    Q_INVOKABLE void restoreList();

    /*! 艺人资料页：拉这个艺人的歌（进独立模型 artistSongs/artistAlbums） */
    Q_INVOKABLE void loadArtist(const QString &name);
    /*! 播放艺人页第 index 首（先拷成当前播放列表，next/prev 才管用） */
    Q_INVOKABLE void playArtistSong(int index);
    /*! 播放推荐页第 index 首 */
    Q_INVOKABLE void playRecommendSong(int index);
    /*!
     * 播放【浏览列表】里第 index 首（歌单 / 专辑 / 搜索结果页点歌曲）。
     * ★ 这一步才把浏览列表拷成播放队列（setPlaylistFrom + playIndex），
     *   之前只是看看，不动队列。
     */
    Q_INVOKABLE void playBrowseSong(int index);
    /*! 浏览列表第 index 首的评论（和 requestCommentsForIndex 的区别在取数模型） */
    Q_INVOKABLE void requestCommentsForBrowseIndex(int index);

    /*!
     * 搜索页专用：播放 / 取评论。
     * ★ 必须基于【独立模型 m_searchSongs】取数，不能用 browse 下标 ——
     *   搜索页显示的是搜索那一刻的快照，用户之后打开歌单/专辑就把浏览列表
     *   换掉了，按 browse 下标取会取到不相干的歌（真机反馈）。
     */
    Q_INVOKABLE void playSearchSong(int index);
    Q_INVOKABLE void requestCommentsForSearchIndex(int index);

    /*!
     * 「音乐详情」(Properties) 页的数据：一行一个 { title, description }。
     * 歌曲名 / 作者 / ID / 长度 / 所属专辑 直接来自 currentTrack；
     * 作词、作曲等在歌词原文里【有才给】，没有就不出那一行。
     * 只有当前播放的曲目才有内容。
     */
    Q_INVOKABLE QVariantList songProperties() const;
    /*!
     * 详情目标那首歌的歌词原文（LRC 文本）。
     * ★ 和 m_lyricRaw（正在播放那首的歌词）是【两份独立数据】——
     *   看列表里别的歌时，绝不能把主界面迷你条的歌词顶掉。
     */
    Q_INVOKABLE QString propertiesLyric() const;

    /*!
     * 封面（Active Frame）专用的封面路径。
     * ★ 和 bigImagePath 同一来源，区别是它会打【限量诊断日志】—— 用来判断
     *   封面"卡住 / 不刷新"到底是哪一环出的问题：
     *     · 日志完全不出现         → cover 的绑定根本没求值（content 没渲染）
     *     · 一直打 (未就绪)        → 640px 大图还没下好（"卡几秒"多半是它）
     *     · 从 (未就绪) 变成本地路径 → 图下好后绑定也跟着重算了（正常）
     *   只在该值真正变化时打一条，不会刷屏。
     */
    Q_INVOKABLE QString coverArtPath(const QString &url);

    /*!
     * ★★ 封面（多任务视图 SceneCover）要用的数据，一律在 C++ 侧算好，
     *    并且【全部是基本类型】（QString / bool）—— 没再用 QVariantMap。
     *
     *   为什么不在 QML 里算：SceneCover 的 content 跑在【独立的 QML 上下文】里，
     *   多行 JS 绑定在那儿求值容易出岔子 —— 一旦异常，整片 content 就渲染不出来，
     *   表现是多任务视图里【一片黑】。真机反馈：同一首歌、仅仅关掉「歌词显示」，
     *   封面就全黑（打开就正常），而歌词开关只该影响文本。
     *
     *   为什么拆开而不是打包成一个 QVariantMap：map 类型的属性每次求值都会
     *   返回一个【新对象】，在 cover 那个独立上下文里是可疑的不稳定因素；
     *   拆成几个 QString / bool 之后就只是最普通的属性读取了。
     *
     *   coverImagePath 为空 = 没有封面，QML 那边自己铺一块深色底 + 居中音符。
     *   ★ 别改成"没封面就给 ic_default.png"：那张图是【深灰底 + 黑音符】，
     *     AspectFill 铺满之后看着就是一片黑，会被当成 bug（实测踩过）。
     *
     *   NOTIFY 统一用 coverInfoChanged：曲目、歌词、翻译、歌词/翻译开关、
     *   图片缓存任一变化都会发（见各处 emit）。
     */
    /*!
     * 封面图的路径（给多任务视图封面 ImageView 用）。
     * ★★ 返回的是【共享目录】里的副本路径，不是应用沙箱里的原文件！
     *   cover 的内容由系统服务渲染，它读不到 /accounts/1000/appdata/...，
     *   只读得到 /accounts/1000/shared/...（需要 access_shared 权限）。
     *   读不到时返回 asset:///images/ic_default.png 兜底。
     */
    Q_PROPERTY(QString coverImagePath READ coverImagePath NOTIFY coverInfoChanged)
    QString coverImagePath();
    /*!
     * 内部用：真实的封面路径，没有封面时返回【空串】。
     * ★ coverImagePath 在它为空时会兜一张占位图而不是空串 —— 原因见 cpp 里的说明
     *   （imageSource 从空变非空的那次赋值，SceneCover 不会重绘）。
     */
    QString coverRealPath();

    /*!
     * 「封面已经可以换成真图了」—— 见 coverImagePath 里那段说明：
     * 起播时先把 imageSource 设成 asset:// 占位（保证能渲染），随后再切成
     * 共享目录里的真封面，用这次【值变化】逼 SceneCover 重绘。
     * 曲目切换时复位。
     */
    bool m_coverReady;
    /*! 诊断用：上一次 coverImagePath 返回的值（变化时才打日志） */
    QString m_coverDiagPath;
    /*!
     * 「轻轻推一下」封面的计数器（见 nudgeCover）。
     * 奇数时 coverTitle 会带一个零宽空格 —— 肉眼不可见、不影响排版，
     * 但能让值发生变化，从而驱动 SceneCover 重绘一次。
     */
    int m_coverNudge;
    Q_PROPERTY(bool coverHasArt READ coverHasArt NOTIFY coverInfoChanged)
    bool coverHasArt();
    Q_PROPERTY(QString coverTitle READ coverTitle NOTIFY coverInfoChanged)
    QString coverTitle();
    Q_PROPERTY(QString coverSubtitle READ coverSubtitle NOTIFY coverInfoChanged)
    QString coverSubtitle();
    /*!
     * 详情页目标的版本号：目标变化、或它的歌词异步到达时 +1。
     * QML 绑它重新取快照（歌词是异步来的，进页面时常常还没到）。
     */
    Q_PROPERTY(int propertiesVersion READ propertiesVersion NOTIFY propertiesChanged)
    int propertiesVersion() const { return m_propertiesVersion; }
    /*! 打开列表里【指定那一首】的音乐详情（歌曲项长按菜单用） */
    Q_INVOKABLE void requestOpenPropertiesFor(const QVariantMap &song);

    /*! ★ 必须是 Q_INVOKABLE：设置页的 RadioGroup 要调它，
     *   漏了 Q_INVOKABLE 时 QML 里是 "music.setBitrate is not a function" */
    Q_INVOKABLE void setBitrate(int br);

    /*! 设置是否隐藏 VIP 歌曲：写 QSettings 持久化 + 立即重填歌曲列表 */
    Q_INVOKABLE void setHideVip(bool hide);

    /* ---- QML 可调用的入口 ---- */

    /*! 从磁盘恢复登录态与设置（应用启动时调用一次） */
    Q_INVOKABLE void init();

    /*!
     * 手动粘贴登录。输入可以是纯 MUSIC_U 值或整段浏览器 cookie。
     * 立即返回；结果通过 loggedIn / nickName / lastLoginError 反映
     * （会自动调一次 /nuser/account/get 校验）。
     */
    Q_INVOKABLE void login(const QString &cookieInput);

    /*! 退出登录（退出后账号仍留在「已登录」列表里，随时能再登回来） */
    Q_INVOKABLE void logout();

    /* ---- 账号管理（多账号） ---- */

    /*!
     * 切到另一个已保存的账号（账号管理页长按「登录」）。
     * 换完凭证会先清掉旧账号的数据，再重新校验一次（校验回来自动拉新账号的歌单等）。
     * @return 找到并切换成功返回 true
     */
    Q_INVOKABLE bool switchAccount(const QString &id);

    /*! 删除一个已保存的账号（长按「删除」）；删的正好是当前账号时同时退登 */
    Q_INVOKABLE bool removeAccount(const QString &id);

    /*!
     * 取某个账号的 MUSIC_U 明文（「复制凭据」用）。
     * ★ 明文只在 C++ 里过一手，QML 拿到后立刻交给 copyToClipboard，
     *   不进任何模型/不落日志。
     */
    Q_INVOKABLE QString accountCredential(const QString &id) const;

    /*! 请求打开登录页（账号管理页的「新增账号」用，走 openRequest 机制） */
    Q_INVOKABLE void requestOpenLogin();

    /*! 搜索单曲（结果会替换 songs） */
    Q_INVOKABLE void search(const QString &keyword);

    /*! 加载歌单（结果会替换 songs 并更新 playlistInfo） */
    Q_INVOKABLE void loadPlaylist(const QString &playlistId);

    /*!
     * 拉取我的歌单列表（结果进 playlists）。
     * 需要登录；uid 不明时会先走一次账号校验再自动拉取。
     */
    Q_INVOKABLE void loadMyPlaylists();

    /*! 我的最爱歌单 ID（登录后有效，空 = 没有/未登录） */
    Q_INVOKABLE QString favoritePlaylistId() const;

    /*! 推荐页：拉推荐歌单列表（匿名可用） */
    Q_INVOKABLE void loadDiscover();

    /*! 每日推荐歌曲（★需要登录）。结果替换 songs，可直接播放 */
    Q_INVOKABLE void loadRecommendSongs();

    /*! 我的页：拉等级/听歌数 + 关注的艺人（需登录） */
    Q_INVOKABLE void loadMyProfile();

    /*! MV：拉列表。kind = "first"（最新）或 "all"（全部） */
    Q_INVOKABLE void loadMvList(const QString &kind);

    /*! MV：取第 index 个 MV 的播放地址（结果进 mvUrl） */
    Q_INVOKABLE void requestMvUrl(int index);

    /*!
     * 直接播放某首歌关联的 MV（列表里长按曲目 → "播放 MV"）。
     * mvId 来自歌曲 map；name/artist 用于播放页标题。
     */
    Q_INVOKABLE void openMv(const QString &mvId, const QString &name,
                            const QString &artist);

    /*
     * 音乐云盘【已移除】：2026-10 官方客户端里已经找不到入口，
     * 接口虽然还通（/api/v1/cloud/get 返回 200），但不再是有效功能。
     * （API 层的 fetchCloud / parseCloud 保留着，万一哪天回来能直接用）
     */

    /*! 设置页：清掉图片缓存，@return 删掉的文件数 */
    Q_INVOKABLE int clearImageCache();

    /*! 设置页：清掉我的歌单的本地缓存 */
    Q_INVOKABLE void clearPlaylistCache();

    /*! 设置页：缓存信息（目录 / 文件数 / 字节数） */
    Q_INVOKABLE QString cacheInfo() const;

    /*
     * 设置页：音频（歌曲文件）缓存上限，单位 MB，0 = 不限。
     *
     * ★★ 必须和上面几个一样声明在【public 区】！
     *   放在 private 区时 QML 侧会报
     *     "music.setAudioCacheLimit is not a function" / "…audioCacheLimit…"
     *   （方法没进元对象，QML 找不到 —— 真机日志实测）。
     */
    Q_INVOKABLE void setAudioCacheLimit(int mb);
    Q_INVOKABLE int audioCacheLimit() const;
    /*! 设置页：清空音频缓存，@return 删掉的文件数 */
    Q_INVOKABLE int clearAudioCache();

    /*! 评论：拉当前播放曲目的评论（结果进 comments，不自动开页） */
    Q_INVOKABLE void loadCommentsForCurrent();

    /*!
     * 评论：拉当前播放曲目的评论【并自动打开评论页】。
     * 页面里的「评论」按钮用这个（loadCommentsForCurrent 只拉数据不开页，
     * 从播放列表页/正在播放页点进去会显得"没反应"）。
     */
    Q_INVOKABLE void requestCommentsForCurrent();

    /*! 清空当前曲目列表（切换歌单 / 重新搜索时，避免残留上一份内容） */
    Q_INVOKABLE void clearSongs();

    /*! 评论：拉列表中第 index 首的评论（长按菜单里用，不必先播放） */
    Q_INVOKABLE void loadCommentsForIndex(int index);

    /*!
     * 长按菜单里「查看评论」用：拉评论 + 记下"拉完要自动打开评论页"。
     *
     * ★ 为什么不能在 QML 里直接 open 评论页：
     *   长按菜单在 ListItemComponent（列表项委托）里，委托组件解析不到
     *   外层页面的 id（commentsSheet），直接写会在运行时 ReferenceError
     *   然后静默失效。所以让 C++ 记一个标志，页面层用
     *   onCommentsVersionChanged（属性+版本号的绑定模式，本项目的通行做法）
     *   检测到标志再打开。
     */
    Q_INVOKABLE void requestCommentsForIndex(int index);

    /*!
     * 评论：按歌曲 id 拉并自动打开评论页（「全部歌曲」页长按菜单用）。
     * ★ 全部歌曲的模型是 GroupDataModel，ListItem.indexPath 是 [组, 项] 两层，
     *   indexPath[0] 拿到的是【组下标】而不是歌，所以那一页必须按 id 走这个。
     */
    Q_INVOKABLE void requestCommentsById(const QString &id);

    /*!
     * 歌单页的「评论」按钮用：拉【当前歌单】的评论并自动打开评论页。
     *
     * ★ 为什么不能用 requestCommentsForCurrent：那个要「正在播放的曲目」，
     *   用户在歌单页只是浏览、没播任何歌时它只会弹「先播放一首歌曲」，
     *   表现就是"评论点不开"（真机反馈）。歌单页要看的是歌单自己的评论，
     *   资源 id 前缀 A_PL_0_（见 NmApi::fetchPlaylistComments）。
     */
    Q_INVOKABLE void requestPlaylistComments();

    /*! 配合 requestCommentsForIndex：页面打开后清掉标志 */
    Q_INVOKABLE void clearCommentsAutoOpen();

    /*! 清空评论列表（切换歌曲时用，避免短暂显示上一首歌的评论） */
    Q_INVOKABLE void clearComments();

    /*!
     * 拉【当前 MV】的评论（MV 页的「评论」分段用）。
     * 结果照样进 music.comments 模型，复用歌曲评论那套解析与信号。
     */
    Q_INVOKABLE void requestMvComments();
    Q_PROPERTY(bool commentsAutoOpen READ commentsAutoOpen NOTIFY commentsChanged)
    bool commentsAutoOpen() const { return m_commentsAutoOpen; }

    /*!
     * 列表是否正在滚动（QML 用 ListScrollStateHandler 汇报）。
     * 滚动中不做模型刷新 —— 边滚边 replace() 会让滑动明显卡顿。
     */
    Q_INVOKABLE void setScrolling(bool scrolling);

    /*!
     * 取封面等图片的本地缓存路径；未下载完返回空串并触发下载。
     * 下载完成后 imageCacheVersion 变化，QML 重新求值绑定即可显示。
     * （封面 URL 直接给 ImageView 会报 "Unsupported scheme (http)"，
     *  BB10 的图片加载器不吃网络路径。）
     */
    Q_INVOKABLE QString imagePath(const QString &url);

    /*!
     * 正在播放页【大封面】专用的图片路径。
     *
     * ★ 为什么要单独一份：imagePath 走的那份缓存为了列表滑动流畅，落盘前
     *   会把图缩到 160px（见 NmImageCache::setMaxImageSize）；而播放页的封面
     *   是按 76.8du 显示的（≈230~250px），拿 160px 的图放大必然发糊。
     *   所以这里另开一份「大图缓存」（边长 640），只服务这一个地方。
     *   ★ 两份缓存的文件名带尺寸后缀（fileNameFor 里的 -sxxx），互不覆盖。
     */
    Q_INVOKABLE QString bigImagePath(const QString &url);

    /*!
     * 播放列表中第 index 首。
     * 成功拿到播放地址后 playUrlVersion 会变化，QML 侧据此起播。
     */
    Q_INVOKABLE void playIndex(int index);

    /*! 下一首 / 上一首（循环）。等价于 playIndex(currentIndex±1)。 */
    Q_INVOKABLE void next();
    Q_INVOKABLE void prev();

    /*!
     * 把一首歌插到【当前曲目的下一位】（长按菜单的「下一首播放」）。
     *
     * ★ 本项目的「播放队列」就是 m_songs 本身（next/prev 都按它的下标走），
     *   所以这里就是往 currentIndex 后面插一项。当前没有任何列表在播时，
     *   退化成"直接起播这一首"——否则这个按钮看着像没反应。
     *
     * @param song 歌曲 map。QML 里直接把 ListItemData 传进来，不用再按 id 查，
     *             这样「全部歌曲」那种两层 indexPath 的模型也能照用。
     * @return 真正插进队列返回 true；已经在下一首、或 id 无效则 false。
     */
    Q_INVOKABLE bool playNextSong(const QVariantMap &song);

    /*!
     * 请求打开「播放队列」页（正在播放页的长按菜单用）。
     *
     * ★ 走的是和 requestOpenPlaylist 同一套「请求 + 版本号」机制：
     *   NowPlayingPage 是 createObject 推出来的独立页，拿不到 main.qml 的 root，
     *   不能直接推页；由 main.qml 的 onOpenRequestVersionChanged 代劳
     *   （kind == "queue"）。
     */
    Q_INVOKABLE void requestOpenQueue();
    /*! 二级页面请求打开「正在播放」页（下拉菜单用，kind="nowplaying"） */
    Q_INVOKABLE void requestOpenNowPlaying();
    /*! 请求打开「音乐详情」(Properties) 页：看当前播放这首歌的信息 */
    Q_INVOKABLE void requestOpenProperties();

    /*!
     * 清空播放队列（队列页的「清空」）。
     * ★ 只清列表、不停播：正在放的这首继续放（和 clearSongs() 同一套约定）。
     */
    Q_INVOKABLE void clearQueue();

    /*!
     * 从队列里删掉一首（队列页每项的「删除」）。
     * ★ 删的是正在播的那首时也【不停播】，只把下标往前挪一位，
     *   保证 next() 接着放它后面那首。
     */
    Q_INVOKABLE void removeSongAt(int index);

    /*!
     * 复制文本到系统剪贴板（评论区长按「复制」用）。
     * ★ 成功与否由调用方（QML）自己弹提示 —— C++ 里写中文要走 NmTr 的十六进制，
     *   没必要为了四个字折腾。
     */
    Q_INVOKABLE void copyToClipboard(const QString &text);

    /*!
     * 读系统剪贴板里的纯文本（登录页「粘贴」用）。
     * ★ 剪贴板里没有文本时返回空串 —— 调用方据此提示"剪贴板是空的"，
     *   不要在这里弹提示（C++ 写中文要 NmTr 十六进制，没必要）。
     */
    Q_INVOKABLE QString clipboardText() const;

    /*!
     * 请求打开某个用户的资料页（评论区长按「打开用户资料」）。
     *
     * ★ 昵称/头像由调用方直接从评论数据里带过来：页面一推出来就能显示，
     *   不用等 /user/playlist 回来（否则会先闪一下空头像白页）。
     *   走和 requestOpenPlaylist 同一套「请求 + 版本号」机制，由 main.qml 推页。
     */
    Q_INVOKABLE void requestOpenUser(const QString &uid,
                                     const QString &nickName,
                                     const QString &avatarUrl);

    /*! 拉「正在查看的那个用户」的歌单（结果进 viewedPlaylists） */
    Q_INVOKABLE void loadViewedPlaylists(const QString &uid);

    /*! 清空错误信息（UI 关闭提示条时调用） */
    Q_INVOKABLE void clearError();

    /*!
     * 给 QML 用的提示入口：页面创建失败这类问题如果不提示，
     * 表现出来就是"点了没反应"，很难查。
     */
    Q_INVOKABLE void notifyError(const QString &text);

    /*!
     * 弹一个系统 Toast（轻提示）。
     *
     * ★★ 为什么必须补上它：notifyError 原来只调 setError()，而整个界面里
     *    【没有任何地方绑定 music.errorMessage】—— 等于"文案写了，但没人显示"，
     *    表现就是「点了没反应」（真机反馈：点同一首歌应该出提示却什么都没有）。
     *    现在 notifyError 除了记 errorMessage，还会弹一个系统 Toast。
     *
     * ★ Toast 对象必须【常驻】（m_toast）：show() 是异步的，局部对象在
     *   函数返回时就析构了，提示根本来不及画出来（BBTieba 踩过同一个坑）。
     */
    Q_INVOKABLE void showToast(const QString &body);

    /*!
     * 给 QML 用的调试追踪：打到 stderr，调试终端里能直接看到。
     * 页面推不出来这类问题，靠它定位卡在哪一步。
     */
    Q_INVOKABLE void trace(const QString &text);

    /*! 接口调试模式（记录请求与响应原文，MUSIC_U 已打码） */
    Q_INVOKABLE void setDebugEnabled(bool enabled);
    Q_INVOKABLE bool debugEnabled() const;
    Q_INVOKABLE QString debugLog() const;
    Q_INVOKABLE void clearDebugLog();

    /*! 控制台日志开关：把接口日志实时打到 stderr（真机排查用） */
    Q_INVOKABLE void setConsoleLogEnabled(bool enabled);
    Q_INVOKABLE bool consoleLogEnabled() const;

    /*! 当前列表的曲目数（QML 显示用） */
    Q_INVOKABLE int songCount() const;

    /*!
     * 在当前列表里做关键词过滤（播放列表页的「歌单内搜索」用）。
     * 只影响显示用的 songs 模型；m_songMaps 保持完整，
     * 所以过滤后 playIndex 的下标仍然是过滤后的下标（见实现里的说明）。
     */
    Q_INVOKABLE void setListFilter(const QString &text);

    /*!
     * 当前列表的标题：歌单名，或者上一次搜索的关键词。
     * 播放列表页用它当标题（歌单 / 专辑 / 艺人搜索结果都复用这一个页面）。
     */
    Q_PROPERTY(QString listTitle READ listTitle NOTIFY songsChanged)
    QString listTitle() const;

    /*!
     * 当前列表的曲目数（歌单/专辑页头部那个"N 首"）。
     *
     * ★ 和 songCount() 是同一个值，但这个【带 NOTIFY】——
     *   QML 里把 songCount() 这种 Q_INVOKABLE 写进绑定只会算一次：
     *   页面是 createObject 出来才联网拉数据的，求值时列表还空着，
     *   于是"0 首"永远不刷新（真机反馈：专辑/歌单页头都显示 0 首）。
     */
    Q_PROPERTY(int songTotal READ songTotal NOTIFY songsChanged)
    int songTotal() const;

signals:
    void songsChanged();
    void playlistsChanged();
    /*! 正在查看的别人资料 / 他的歌单（评论区长按「打开用户资料」用） */
    void viewedUserChanged();
    void viewedPlaylistsChanged();
    /*! 当前账号 / 已保存账号列表变了（账号管理页据此刷新） */
    void accountsChanged();
    /*! 「资料库 → 艺术家」索引变了 */
    void libraryArtistsChanged();
    /*! 「资料库 → 播放列表」分组模型变了 */
    void libraryPlaylistsChanged();
    void discoverChanged();
    void albumsChanged();
    void commentsChanged();
    void imageCacheChanged();
    void playlistInfoChanged();
    void profileChanged();
    void artistsChanged();
    void browseSongsChanged();
    /*!
     * 封面四个属性（coverImagePath / coverHasArt / coverTitle / coverSubtitle）
 * 的 NOTIFY —— 曲目、歌词、翻译、歌词/翻译开关、图片缓存
     * 任一变化都会发，让多任务视图封面（SceneCover）跟着刷新。
     */
    void coverInfoChanged();
    void lyricsEnabledChanged();
    void currentLyricLineChanged();
    void lyricsTransEnabledChanged();
    void currentLyricTransChanged();
    void mvsChanged();
    void allSongsChanged();
    void mvUrlChanged();
    void currentTrackChanged();
    void propertiesChanged();
    void playUrlChanged();
    void playerPositionChanged();
    void loadingChanged();
    void repeatModeChanged();
    void shuffleChanged();
    void errorMessageChanged();
    void errorDetailChanged();
    void lastRawChanged();
    void loginChanged();
    void lastLoginErrorChanged();
    void bitrateChanged(int br);
    void themeChanged();
    void hideVipChanged();
    void openRequestChanged();
    void artistSongsChanged();
    void recommendSongsChanged();
    void searchSongsChanged();

    /*! 一次业务请求结束（成功或失败都会发） */
    void requestFinished(const QString &what, bool success);

private slots:
    /*!
     * 曲目变化后延迟再补发一次 coverInfoChanged。
     * ★ 真机反馈：起播后到歌词到达之间，多任务视图的封面是一块黑，歌词一上来
     *   就恢复了 —— 说明只是"那一下"没重绘。补一次通知等价于人为制造一次内容
     *   变化，把这段窗口填上。见 cpp 里的调用点。
     */
    void notifyCoverAgain();
    /*!
     * 起播后隔一会儿"推"封面几次（见 cpp 里的说明）。
     * ★ 只改一个看不见的零宽空格，用来驱动 SceneCover 的那一次重绘；
     *   没有它的话，没歌词 / 未在播放这种"静态"状态下封面永远画不出来。
     */
    void nudgeCover();
    /*! 搜索防抖：停手 300ms 后才真正发请求（见 search()） */
    void onSearchDebounce();
    /*! 歌词接口回来（见 fetchLyric） */
    void onLyricFinished(int requestId, const nm::NmParsers::LyricParse &result);

    /*!
     * 一首播完 —— 由 applicationui 直接连到播放器的 playbackCompleted()。
     * （播放器现在是 C++ 的 context property，QML 里没法写
     *   onPlaybackCompleted 处理器，所以放这里。
     *   Qt 4 用 SIGNAL/SLOT 字符串连接，不受 private 限制。）
     */
    void playbackCompleted();

    /*!
     * 应用被缩到后台（bb::Application::thumbnail）。
     *
     * ★ 只做记录 + 日志，不做任何会打断播放的动作 —— 音乐 app 就是要
     *   在后台继续放。缺 run_when_backgrounded 时进程在这里之后会被挂起，
     *   日志会停在 thumbnail 不再往下走，这是判断"有没有真的后台运行"
     *   最直接的一条依据。
     */
    void onAppThumbnailed();

    /*!
     * 应用回到前台（bb::Application::fullscreen）。
     *
     * ★ 兜底：如果上一首其实已经播完、但 playbackCompleted 没被处理到
     *   （典型是进程在后台被挂起期间事件投递不到），在这里补切一次，
     *   免得用户回到前台看到"停在歌尾、不往下走"。
     *   判定很严格（见 cpp 里的注释），绝不会对"用户主动暂停/停止"误触发。
     */
    void onAppForegrounded();

    /*! 播放器 positionChanged 信号回调（真机），更新 playerPosition 通知 QML */
    void onPlayerPositionChanged(unsigned int position);
    /*! 轮询回调：读 position() 兜底（模拟器上 positionChanged 不触发） */
    void onPositionTick();

    void onSearchFinished(int requestId, const nm::NmParsers::SearchParse &result);
    void onPlaylistFinished(int requestId, const nm::NmParsers::PlaylistParse &result);
    void onAlbumFinished(int requestId, const nm::NmParsers::PlaylistParse &result);
    void onSongUrlFinished(int requestId, qint64 songId,
                           const nm::NmParsers::SongUrlParse &result);
    void onAccountFinished(int requestId, const nm::NmParsers::AccountParse &result);
    void onUserPlaylistsFinished(int requestId,
                                 const nm::NmParsers::UserPlaylistsParse &result);
    /*! 封面下载完成：版本号 +1，并把多次完成合并成一次模型刷新 */
    void onImageReady();
    /*!
     * 大封面（播放页）下载完成。
     * ★ 只顶版本号让 QML 重新求值，【不排】那次全模型扫描 —— 大图不进任何
     *   模型字段（播放页是直接调 bigImagePath 拿路径的），扫了也是白扫。
     */
    void onBigImageReady();
    /*!
     * 把已下载图片的本地路径刷进列表模型。
     * ★ 必须是 slot：这里是用 SLOT() 字符串连接到定时器的 timeout()
     *   的，写成普通成员函数的话 connect 在运行时静默失败
     *   （"图片下载完了但列表永远不显示" —— BBTieba 踩过同款坑）。
     */
    void refreshImagePaths();
    void onSongDetailFinished(int requestId,
                              const nm::NmParsers::SongDetailParse &result);
    void onDiscoverFinished(int requestId, const nm::NmParsers::DiscoverParse &result);
    void onRecommendFinished(int requestId, const nm::NmParsers::DiscoverParse &result);
    void onCommentsFinished(int requestId, const nm::NmParsers::CommentsParse &result);
    void onLevelFinished(int requestId, const nm::NmParsers::LevelParse &result);
    /*! 别人的资料回来了（等级/听歌数/昵称/头像/签名）→ 补进 m_viewedUser */
    void onUserDetailFinished(int requestId,
                              const nm::NmParsers::UserDetailParse &result);
    void onArtistsFinished(int requestId, const nm::NmParsers::ArtistsParse &result);
    void onMvsFinished(int requestId, const nm::NmParsers::MvsParse &result);
    void onMvUrlFinished(int requestId, const nm::NmParsers::MvUrlParse &result);
    void onRequestFailed(int requestId, const QString &tag, const QString &message,
                         const QVariantMap &detail);
    /*! 延迟推页的定时器到点：这时才真正把 openRequestChanged 发出去 */
    void onOpenRequestTimeout();
    /*!
     * 把 NmSession 里的"已保存账号"同步到 m_otherAccounts 模型。
     * 登录/切换/删除/退出后都会走它（也直接连到 session 的 accountsChanged）。
     */
    void rebuildAccounts();

private:
    void setLoading(bool value);
    void setError(const QString &message, const QVariantMap &detail = QVariantMap());
    /*! 按 m_theme 应用视觉风格（"" 时不动） */
    void applyThemeStyle();
    /*!
     * 当前搜索要填哪个歌曲模型：
     *   艺人模式 → artistSongs；推荐模式 → recommendSongs；否则 → 全局 songs
     */
    bb::cascades::ArrayDataModel *songsTarget() const
    {
        if (m_artistMode)
            return m_artistSongs;
        if (m_recommendMode)
            return m_recommendSongs;
        if (m_browseMode)
            return m_browseSongs;
        return m_songs;
    }
    QVariantList &songsTargetMaps()
    {
        if (m_artistMode)
            return m_artistSongMaps;
        if (m_recommendMode)
            return m_recommendSongMaps;
        if (m_browseMode)
            return m_browseSongMaps;
        return m_songMaps;
    }
    /*!
     * 只清【浏览列表】 m_browseSongs，绝不动 m_songs（播放队列）。
     * 见 clearSongs() —— 那个会连队列一起清，浏览场景不能用它。
     */
    void clearBrowseList();

    /*! 按当前播放进度，把 m_currentLyricLine 更新到对应的那句歌词 */
    void updateLyricForPosition(int msec);

    /*! 这首歌在当前「隐藏 VIP」设置下是否应显示 */
    bool songVisible(const QVariantMap &map) const;
    /*! 按当前「隐藏 VIP」设置，把三个歌曲模型从各自的完整 maps 重填一遍 */
    void refillSongModels();
    /*! 由 artistSongs 聚合出 artistAlbums */
    void buildArtistAlbums();
    /*! 把目标列表拷成当前播放列表（next/prev 用 m_songMaps） */
    void setPlaylistFrom(const QVariantList &maps);
    void finishRequest(const QString &what, bool success);
    /*!
     * 把「打开某页」的通知推迟一小会儿再发给 QML。
     *
     * ★★ 为什么：从长按菜单（ActionSet）点进来的那一下，菜单关闭的 peek
     *   动画会被 NavigationPane 当成"向左滑返回"，刚推上去的页立刻被判
     *   isSuggestedStackOk 弹回来（真机日志：push 完马上 popped page），
     *   用户侧的表现就是"同一下要点两次才进得去"。
     *   等动画走完再通知，一次就能进。
     *
     * ★ 也试过在 QML 里用 Timer 做延迟 —— 但 BB10 的 bb.cascades 里【没有
     *   Timer 这个类型】（官方 Sample 那个是 import Timer 1.0 的自定义类型），
     *   直接写 Timer 会让 main.qml 加载失败、整个界面黑屏。所以放在 C++ 里做。
     */
    void scheduleOpenRequest();
    void fillSongsModel(const QList<nm::NmSong> &songs);
    /*! 填「全部歌曲」：写入完整 maps，再重建（排序 + 字母分组 header） */
    void fillAllSongsModel(const QList<nm::NmSong> &songs);
    /*! 从 m_allSongMaps 重建「全部歌曲」模型（VIP 过滤 + 排序 + 插 header） */
    void rebuildAllSongs();
    /*! 从 m_allSongMaps 重建「艺术家」索引（按艺人分组，见 libraryArtists） */
    void rebuildLibraryArtists();
    /*! 从 m_playlists 重建资料库的歌单分组模型（我创建的 / 我收藏的） */
    void rebuildLibraryPlaylists();
    void loadAllSongsCache();
    void saveAllSongsCache();
    void resetPlayer();
    /*! 用 /api/song/detail 的结果补齐列表里缺失的封面地址 */
    void mergeArtUrls(const QList<nm::NmSong> &details);
    /*!
     * 列表里有曲目缺封面时，发一次 /api/song/detail 去补齐。
     * @return 是否发了请求（调用方据此决定要不要等它回来再收尾）
     */
    bool requestArtEnrichmentIfNeeded();
    void fillCommentsModel(const QList<nm::NmComment> &hot,
                           const QList<nm::NmComment> &normal);
    /*! 按 albumId 把当前曲目聚合成专辑列表（见 albums 属性的说明） */
    void buildAlbumsFromSongs();
    /*! 我的歌单的本地缓存：启动时先显示缓存，联网再刷新 */
    void loadPlaylistCache();
    void savePlaylistCache();
    /*! 记一条本地播放记录（playIndex 里调） */
    void recordRecentPlayed(const QVariantMap &song);
    /*! 歌单曲目缓存：有缓存返回 true 并填入 songs；没有返回 false */
    bool loadPlaylistSongsCache(const QString &playlistId);
    void savePlaylistSongsCache(const QString &playlistId);

    /*! 推荐歌单 / MV / 歌手 的本地缓存：和我的歌单同思路，
     *  启动先显示本地缓存、联网后再刷新，避免每次打开软件都重新拉取。 */
    void loadDiscoverCache();
    void saveDiscoverCache();
    void loadMvCache();
    void saveMvCache();
    void loadArtistsCache();
    void saveArtistsCache();

    nm::NmHttpClient *m_http;
    nm::NmSession *m_session;
    nm::NmApi *m_api;
    nm::NmImageCache *m_imageCache;
    /*! 音频（歌曲文件）落盘缓存：播放的同时后台缓存，见 NmAudioCache */
    nm::NmAudioCache *m_audioCache;
    /*! 大图缓存：只给正在播放页的封面用（见 bigImagePath），边长 640 */
    nm::NmImageCache *m_bigImageCache;
    /*! applicationui 注入的全局播放器（seek 用，可为 0） */
    bb::multimedia::MediaPlayer *m_mediaPlayer;

    /*!
     * 本次"播完"是否已经被处理过（见 onAppForegrounded）。
     *
     *   false = 当前这首还没播完（或播完了但 playbackCompleted 没处理到）
     *   true  = playbackCompleted 已经进来过、已经安排了下一首
     *
     * ★ 这是兜底补切能不能安全触发的关键：只有 false（说明真没收到过
     *   playbackCompleted）时才允许补切，避免正常切歌途中被误判成"卡住"
     *   而连跳两首。起播新曲目（playIndex）时清回 false。
     */
    bool m_advanceHandled;

    /*! 列表快照（见 snapshotList / restoreList） */
    QVariantList m_snapshotSongs;
    QVariantList m_snapshotAlbums;
    QVariantMap m_snapshotPlaylistInfo;
    QString m_snapshotLastQuery;
    bool m_hasSnapshot;
    /*!
     * 「当前列表是否已被用户接管为播放队列」。
     * ★ 进歌单/专辑页会先 snapshot；离开时【只有没接管过】才还原 ——
     *   否则用户点了播放、队列刚变成这个歌单，一返回又被还原掉了。
     *   置位时机：playIndex() / setPlaylistFrom()（即真正开始播）。
     */
    bool m_queueAdopted;
    /*! 请求去重：正在拉「我的歌单」/「我的页资料」时不再重复发 */
    bool m_playlistsFetching;
    bool m_profileFetching;

    /*! 艺人资料页的独立模型（见 artistSongs 属性） */
    bb::cascades::ArrayDataModel *m_artistSongs;
    QVariantList m_artistSongMaps;
    bb::cascades::ArrayDataModel *m_artistAlbums;
    int m_artistSongsVersion;
    int m_artistAlbumsVersion;
    /*! 正在为艺人页做搜索（fill/enrich/merge 都据此改目标模型） */
    bool m_artistMode;
    /*!
     * 浏览模式：歌单 / 专辑【只是打开来看】时置位。
     * ★ 填进 m_browseSongs 而不是全局 m_songs —— 浏览不动播放队列，
     *   只有点某首播放（playBrowseSong）才把这份列表拷成队列。
     */
    bool m_browseMode;
    /*! 浏览模式的显示模型（见 m_browseMode） */
    bb::cascades::ArrayDataModel *m_browseSongs;
    /*! 浏览模式的完整 maps（VIP 过滤 / 拷成队列都用它） */
    QVariantList m_browseSongMaps;
    int m_browseSongsVersion;
    /*! 搜索防抖定时器（见 search / onSearchDebounce） */
    QTimer *m_searchTimer;
    /*! 防抖期间暂存的关键词 */
    QString m_pendingSearch;
    /*! 歌词开关（设置项，写 QSettings） */
    bool m_lyricsEnabled;
    /*! 当前曲目的歌词原文 / 翻译原文 */
    QString m_lyricRaw;
    QString m_transRaw;
    /*! 解析出的歌词行：时间与文本（下标一一对应） */
    QList<int> m_lyricTimes;
    QStringList m_lyricTexts;
    /*! 当前这句歌词（QML 直接绑它） */
    QString m_currentLyricLine;
    /*! 翻译开关（设置项，写 QSettings） */
    bool m_lyricsTransEnabled;
    /*! 每句歌词对应的翻译（与 m_lyricTexts 下标一一对应，无翻译为空串） */
    QStringList m_lyricTrans;
    /*! 当前这句歌词的翻译 */
    QString m_currentLyricTrans;

    /*! 推荐页独立模型（见 recommendSongs 属性） */
    bb::cascades::ArrayDataModel *m_recommendSongs;
    QVariantList m_recommendSongMaps;
    int m_recommendSongsVersion;
    /*! 正在拉每日推荐（fill/enrich/merge 都据此改目标模型） */
    bool m_recommendMode;

    /*! 搜索页独立模型（见 searchSongs 属性） */
    bb::cascades::ArrayDataModel *m_searchSongs;
    int m_searchSongsVersion;

    /*! 歌单曲目缓存：保存时要知道是哪个歌单 */
    QString m_pendingPlaylistId;

    bb::cascades::ArrayDataModel *m_songs;
    QVariantList m_songMaps;      // 与模型同步的数据源（next/prev 用）
    QString m_lastQuery;          // 上一次搜索的关键词（listTitle 用）

    bb::cascades::ArrayDataModel *m_playlists;
    int m_playlistsVersion;
    /*! 资料库歌单列表的分组版（见 libraryPlaylists） */
    bb::cascades::GroupDataModel *m_libraryPlaylists;
    int m_libraryPlaylistsVersion;

    /*! 正在查看的别人的资料（昵称/头像）。见 requestOpenUser */
    QVariantMap m_viewedUser;
    /*! 正在查看的那个人的歌单（和 m_playlists 分开，见 viewedPlaylists 的说明） */
    bb::cascades::ArrayDataModel *m_viewedPlaylists;
    /*! onUserPlaylistsFinished 的结果该进哪个模型（true = viewedPlaylists） */
    bool m_viewedUserMode;

    bb::cascades::ArrayDataModel *m_discoverPlaylists;
    int m_discoverVersion;

    bb::cascades::ArrayDataModel *m_comments;
    int m_commentsVersion;
    int m_commentTotal;
    /*! 评论页标题（"歌曲评论" / "歌单评论"，见 commentsTitle 属性） */
    QString m_commentsTitle;
    /*! 当前打开的 MV 的 id（MV 评论区要用，见 requestMvComments） */
    qint64 m_currentMvId;

    bb::cascades::ArrayDataModel *m_artists;
    int m_artistsVersion;

    bb::cascades::ArrayDataModel *m_albums;
    int m_albumsVersion;

    bb::cascades::ArrayDataModel *m_mvs;
    int m_mvsVersion;

    /*! 「全部歌曲」：模型（GroupDataModel，按 sectionKey 分组）+ 完整 maps + 版本 */
    bb::cascades::GroupDataModel *m_allSongs;
    QVariantList m_allSongMaps;
    int m_allSongsVersion;
    bool m_allSongsMode;
    /*! 全部歌曲页的页内搜索关键词（见 setAllSongsFilter） */
    QString m_allSongsFilter;
    /*
     * 「全部歌曲」合并拉取（见 loadAllSongs）：
     * 现在要【串行】拉「我创建的所有歌单」（收藏的歌单不算）再合并去重，
     * 所以需要一份待拉 id 队列，和一个跨歌单的累积容器 + 去重表。
     */
    QStringList m_allSongsPendingIds;
    QVariantList m_allSongsFetch;
    QVariantList m_allSongsFetchIds;
    /*
     * ★「全部歌曲」合并拉取【正在进行】的独立标记 + 当前那个请求的歌单 id。
     *   不能拿 m_allSongsMode 兼任：用户中途打开一个普通歌单时，loadPlaylist
     *   会把它清成 false，于是"全部歌曲"剩下的响应会被当成普通歌单处理，
     *   甚至把那些曲目写进该歌单的缓存 —— 真机表现就是"歌单先显示一堆不对的
     *   内容、网络回来再整体换掉"（闪内容）。
     */
    bool m_allSongsFetching;
    QString m_allSongsFetchId;

    /*! 把一批曲目累积进「全部歌曲」的合并结果（跨歌单按 id 去重） */
    void appendAllSongsBatch(const QList<nm::NmSong> &songs);
    /*! 所有歌单都拉完了：和旧数据比对后重建模型 + 写缓存 */
    void finishAllSongsFetch();
    /*! onAlbumFinished 的分支标记：true=追加进队列（见 enqueueAlbum） */
    bool m_enqueueAlbumMode;
    /*!
     * 「资料库 → 艺术家」按艺人归类的索引（见 libraryArtists 属性）。
     * ★ 是 GroupDataModel（按 sectionKey 分组），和「全部歌曲」一样 ——
     *   只有它能让 ListView 认出 "header" 类型项，从而有置顶的分组条
     *   （ArrayDataModel 的 itemType() 恒为空串，做不到）。
     */
    bb::cascades::GroupDataModel *m_libraryArtists;
    int m_libraryArtistsVersion;
    QString m_mvUrl;
    int m_mvUrlVersion;
    QString m_mvName;
    QString m_mvArtist;

    QString m_userAvatarUrl;

    /*! 个人主页背景：启动后第一次选到图就固定下来（避免每次求值都换图） */
    mutable QString m_bannerPath;
    int m_userLevel;
    int m_userPlayCount;
    int m_vipType;
    /*! 评论拉完后页面是否应该自动打开评论页（见 requestCommentsForIndex） */
    bool m_commentsAutoOpen;
    /*! 我的用户 ID（从账号信息来，拉我的歌单要用） */
    qint64 m_userId;
    /*! loadMyPlaylists 时 uid 还没拿到：等账号校验回来再自动拉 */
    bool m_pendingPlaylistsLoad;
    /*! 我的最爱歌单 ID（specialType==5 的那个） */
    QString m_favoritePlaylistId;

    /*! 封面下载完成计数（QML 绑定触发器） */
    int m_imageCacheVersion;
    /*! 是否已排队一次模型刷新（把同时完成的下载合并成一次） */
    bool m_imageRefreshPending;
    /*! 列表是否在滚动（滚动中推迟刷新，见 setScrolling） */
    bool m_scrolling;
    QTimer *m_imageRefreshTimer;

    QVariantMap m_playlistInfo;
    int m_playlistInfoVersion;

    QVariantMap m_currentTrack;
    int m_currentTrackVersion;

    /*! 封面诊断：绑定求值次数 + 上一次的 (url|path)（见 coverArtPath） */
    int m_coverEvalCount;
    QString m_coverDiagLast;

    /*! 详情页当前展示的那首歌（可以是列表里任意一首，未必是正在播放的） */
    QVariantMap m_propertiesTrack;
    /*! 详情页那首歌的歌词原文（独立一份，绝不动 m_lyricRaw） */
    QString m_propertiesLyric;
    /*! 正在为详情页拉歌词 —— onLyricFinished 据此分流 */
    bool m_propertiesLyricFetching;
    int m_propertiesVersion;
    int m_currentIndex;
    /*! 循环模式：1=列表循环 2=单曲循环（默认 1） */
    int m_repeatMode;
    /*! 随机播放开关（独立于 m_repeatMode） */
    bool m_shuffle;
    QString m_playUrl;
    int m_playUrlVersion;
    /*! 播放进度（毫秒）。信号 + 轮询双保险更新 */
    int m_playerPosition;
    /*! 用户是否正在拖动进度条（拖动期间暂停位置更新，防回弹） */
    bool m_seeking;
    /*! 轮询定时器：模拟器上 positionChanged 不触发，靠它兜底推进 */
    QTimer *m_positionTimer;

    /*!
     * ★★ 常驻的 Toast 对象（绝不能写成局部变量！）
     *
     * 用途：notifyError 的轻提示（"已加入下一首"这类）。
     * 为什么常驻：SystemToast::show() 是异步的，交给系统 UI 服务去画；
     *   局部对象在函数返回瞬间就析构了，提示根本来不及显示。
     */
    bb::system::SystemToast *m_toast;

    /*! 「打开某页」通知的延迟定时器（见 scheduleOpenRequest） */
    QTimer *m_openRequestTimer;

    /*! 账号管理页的「已登录」列表（NmSession::otherAccounts() 的模型版） */
    bb::cascades::ArrayDataModel *m_otherAccounts;
    int m_accountsVersion;
    /*! 正在取播放地址的歌曲 id（0 = 没有在取）；用于连点去重 */
    qint64 m_pendingSongUrlId;

    bool m_loading;
    QString m_errorMessage;
    QVariantMap m_errorDetail;
    QString m_lastRaw;

    bool m_loggedIn;
    QString m_nickName;
    QString m_lastLoginError;

    /*! 当前主题："" / "Bright" / "Dark"（见 theme 属性） */
    QString m_theme;

    /*! 隐藏 VIP 歌曲开关（见 hideVip 属性） */
    bool m_hideVip;

    /*! 独立页面的「打开曲目页」请求（见 openRequestVersion 属性） */
    int m_openRequestVersion;
    QString m_openRequestKind;
    QString m_openRequestArg;
};

#endif // MUSIC_CONTROLLER_HPP
