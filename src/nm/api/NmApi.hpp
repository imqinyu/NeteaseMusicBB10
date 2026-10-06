/*
 * NmApi - 网易云音乐老接口的封装（设备直连，无加密）
 *
 * 接口族（base: http://music.163.com/api，全部实测仍存活，2026-10）：
 *
 *   GET /search/get/web            搜索单曲（匿名可用）
 *   GET /v2/playlist/detail        歌单详情（匿名可用）
 *   GET /song/enhance/player/url   播放地址（★必须带 MUSIC_U cookie）
 *   GET /nuser/account/get         登录态检查（带 cookie 返回 profile）
 *
 * 为什么不用 weapi/eapi：那些需要 AES-CBC + RSA 加密，而上面这组
 * 老 GET 接口不加密也还活着 —— MeeGo 版 cloudmusicqt 走的就是这条路。
 *
 * 唯一死掉的部分：老版按 dfsId 拼 CDN 直链的做法（dfsId 现在恒为 0），
 * 所以播放地址必须走 /song/enhance/player/url。
 */

#ifndef NM_API_HPP
#define NM_API_HPP

#include <QMap>
#include <QObject>
#include <QString>

#include "model/NmItems.hpp"
#include "model/NmParsers.hpp"
#include "net/NmHttpClient.hpp"

namespace nm {

class NmCookieStore;
class NmSession;

class NmApi : public QObject
{
    Q_OBJECT
public:
    explicit NmApi(QObject *parent = 0);
    virtual ~NmApi();

    /*! 注入依赖（不获取所有权） */
    void setHttpClient(NmHttpClient *client);
    void setSession(NmSession *session);

    /*! 公共请求头（UA / Referer / Cookie），每个请求都会带 */
    QList<KeyValue> commonHeaders() const;

    /* ---- 业务请求 ---- */

    /*! 搜索单曲 */
    int searchSongs(const QString &keyword, int limit = 30, int offset = 0);

    /*! 歌单详情 */
    int fetchPlaylist(const QString &playlistId);

    /*! 播放地址。br 为期望码率（128000/192000/320000） */
    int fetchSongUrl(qint64 songId, int br);

    /*! 登录态检查 */
    int fetchAccount();

    /*! 我的歌单列表（★必须带 MUSIC_U cookie，uid 从账号信息来） */
    int fetchUserPlaylists(qint64 uid, int limit = 100, int offset = 0);

    /*!
     * 批量取歌曲详情。搜索结果只有 album.picId 没有 picUrl，
     * 只有这个接口会返回可用的 album.picUrl（封面地址）。
     */
    int fetchSongDetail(const QList<qint64> &ids);

    /*! 推荐歌单（/api/personalized/playlist，匿名也能拿） */
    int fetchDiscoverPlaylists(int limit = 20);

    /*! 每日推荐歌曲（/api/discovery/recommend/songs，★需要登录） */
    int fetchRecommendSongs();

    /*! 歌曲评论（/api/v1/resource/comments/R_SO_4_<songId>） */
    int fetchComments(qint64 songId, int limit = 30, int offset = 0);

    /*!
     * 歌单评论（/api/v1/resource/comments/A_PL_0_<playlistId>）
     *
     * ★ 歌单页的「评论」按钮要的是【歌单自己的评论】，不是里面某首歌的。
     *   实测（2026-10）：这个资源 id 前缀返回 code=200 且带 hotComments，
     *   和歌曲评论是同一套响应结构，所以复用 tagComments() 与
     *   commentsFinished 信号，解析也直接复用 parseComments。
     */
    int fetchPlaylistComments(qint64 playlistId, int limit = 30, int offset = 0);

    /*!
     * MV 评论（/api/v1/resource/comments/R_MV_5_<mvId>）
     *
     * ★ 和歌曲评论（R_SO_4_）/ 歌单评论（A_PL_0_）是同一套响应结构，
     *   所以复用 tagComments() 与 commentsFinished 信号、parseComments 解析。
     */
    int fetchMvComments(qint64 mvId, int limit = 30, int offset = 0);

    /*! 用户等级与听歌数（/api/user/level，★需登录） */
    int fetchUserLevel();

    /*!
     * 某个用户的资料：等级 / 听歌数 / 昵称 / 头像 / 签名 / 粉丝数
     *（/api/v1/user/detail/&lt;uid&gt;，★路径式；query 形式实测 404）。
     * 和 artist/sublist 不同，这个接口对【任何用户】都有效。
     */
    int fetchUserDetail(qint64 uid);

    /*! 关注的艺人（/api/artist/sublist，★需登录） */
    int fetchSubscribedArtists(int limit = 20, int offset = 0);

    /*!
     * MV 列表。kind: "first" = 最新 MV，其它 = 全部 MV（/api/mv/all）。
     * 实测（2026-10）：mv/all 有 5000 条；mv/url 与 mv/detail 那两个老接口已死。
     */
    int fetchMvList(const QString &kind, int limit = 30, int offset = 0);

    /*! MV 播放地址（/api/song/enhance/play/mv/url，带时效性签名，★需登录） */
    int fetchMvUrl(qint64 mvId, int r = 480);

    /*! 音乐云盘（/api/v1/cloud/get，★需登录；只做读取，不做上传） */
    int fetchCloud(int limit = 100, int offset = 0);

    /*! 专辑详情（/api/v1/album/{id}）：返回专辑信息 + 曲目 */
    int fetchAlbum(qint64 albumId);

    /*!
     * 歌词（/api/song/lyric?os=pc&id=<id>&lv=-1&kv=-1&tv=-1）
     *
     * ★ lv / kv / tv 都传 -1：让服务端一次把【原词 + 翻译】都返回，
     *   省得再为翻译单独发一次请求。
     */
    int fetchLyric(qint64 songId);

    /*!
     * 扫码登录第一步：取二维码 key（/api/login/qrcode/unikey）
     *
     * ★ 走的是【老接口族】，和参考项目 cloudmusicqt 一致。
     *   新版那套是 /login/qr/key + /login/qr/create + /login/qr/check，
     *   两者的 key 不通用，别混着用。
     */
    int fetchQrKey();

    /*!
     * 扫码登录第二步：轮询二维码状态（/api/login/qrcode/client/login）
     *
     * 返回的 code：800 过期 / 801 待扫码 / 802 待确认 / 803 成功。
     * ★ 803 那一次响应会带 Set-Cookie（MUSIC_U），NmHttpClient 已经统一
     *   收进 cookie 存储了，调用方去 NmCookieStore 取即可。
     */
    int fetchQrStatus(const QString &key);

    /*! 业务 tag 常量（onRequestFailed 里区分来源用） */
    static const char *tagSearch()   { return "search"; }
    static const char *tagPlaylist() { return "playlist"; }
    static const char *tagSongUrl()  { return "songUrl"; }
    static const char *tagAccount()  { return "account"; }
    static const char *tagUserPlaylists() { return "userPlaylists"; }
    static const char *tagSongDetail()    { return "songDetail"; }
    static const char *tagDiscover()      { return "discover"; }
    static const char *tagRecommend()     { return "recommend"; }
    static const char *tagComments()      { return "comments"; }
    static const char *tagLevel()         { return "level"; }
    static const char *tagUserDetail()    { return "userDetail"; }
    static const char *tagArtists()       { return "artists"; }
    static const char *tagMvs()           { return "mvs"; }
    static const char *tagMvUrl()         { return "mvUrl"; }
    static const char *tagCloud()         { return "cloud"; }
    static const char *tagAlbum()         { return "album"; }
    static const char *tagLyric()         { return "lyric"; }
    static const char *tagQrKey()         { return "qrKey"; }
    static const char *tagQrStatus()      { return "qrStatus"; }

signals:
    void searchFinished(int requestId, const nm::NmParsers::SearchParse &result);
    void playlistFinished(int requestId, const nm::NmParsers::PlaylistParse &result);
    void songUrlFinished(int requestId, qint64 songId,
                         const nm::NmParsers::SongUrlParse &result);
    void accountFinished(int requestId, const nm::NmParsers::AccountParse &result);
    void userPlaylistsFinished(int requestId,
                               const nm::NmParsers::UserPlaylistsParse &result);
    void songDetailFinished(int requestId,
                            const nm::NmParsers::SongDetailParse &result);
    void discoverFinished(int requestId, const nm::NmParsers::DiscoverParse &result);
    void recommendFinished(int requestId, const nm::NmParsers::DiscoverParse &result);
    void commentsFinished(int requestId, const nm::NmParsers::CommentsParse &result);
    void levelFinished(int requestId, const nm::NmParsers::LevelParse &result);
    void userDetailFinished(int requestId,
                            const nm::NmParsers::UserDetailParse &result);
    void artistsFinished(int requestId, const nm::NmParsers::ArtistsParse &result);
    void mvsFinished(int requestId, const nm::NmParsers::MvsParse &result);
    void mvUrlFinished(int requestId, const nm::NmParsers::MvUrlParse &result);
    void cloudFinished(int requestId, const nm::NmParsers::CloudParse &result);
    void albumFinished(int requestId, const nm::NmParsers::PlaylistParse &result);
    void lyricFinished(int requestId, const nm::NmParsers::LyricParse &result);
    void qrKeyFinished(int requestId, const nm::NmParsers::QrKeyParse &result);
    void qrStatusFinished(int requestId, const nm::NmParsers::QrStatusParse &result);

    /*! 网络/HTTP 层失败（非 2xx、超时、断网等） */
    void requestFailed(int requestId, const QString &tag, const QString &message,
                       const QVariantMap &detail);

private slots:
    void onHttpFinished(int requestId, const QString &tag,
                        const nm::NmJson &json,
                        const nm::NmHttpResponse &response);

private:
    /*! 发一个 GET。公共头在这里统一注入 */
    int sendGet(const QString &tag, const QString &path,
                const QList<KeyValue> &params);

    NmHttpClient *m_http;
    NmSession *m_session;
    /*! songUrl 请求对应的歌曲 id（响应回传对账） */
    QMap<int, qint64> m_pendingSongIds;
};

} // namespace nm

#endif // NM_API_HPP
