#include "api/NmApi.hpp"

#include <QLatin1Char>
#include <QLatin1String>
#include <QVariantMap>

#include "net/NmCookieStore.hpp"
#include "session/NmSession.hpp"
#include "util/NmString.hpp"

namespace nm {

namespace {

const char kApiBase[] = "http://music.163.com/api";

/*! 把 ids=[a,b] 这种数组参数编码进查询串（网易接口的数组传法） */
QString encodeIdsParam(const QList<qint64> &ids)
{
    QStringList parts;
    foreach (qint64 id, ids)
        parts.append(QString::number(id));
    // ★ 返回原始的 "[id1,id2]"，百分号编码由 NmString::buildQuery 统一做。
    //   之前这里先编了一次（%5B...），buildQuery 又编一次（%255B...），
    //   服务端解出字面量 "%5B...%5D" → 「参数错误」。而 reply->url()
    //   的美化输出恰好只解码一层，日志里完全看不出双重编码。
    return QString(QLatin1String("[%1]")).arg(parts.join(QLatin1String(",")));
}

} // anonymous namespace

NmApi::NmApi(QObject *parent)
    : QObject(parent)
    , m_http(0)
    , m_session(0)
{
}

NmApi::~NmApi()
{
}

void NmApi::setHttpClient(NmHttpClient *client)
{
    m_http = client;

    // ★ 这条 connect 是整个数据链路的第一环，漏了的话请求能发出去、
    //   响应也回来，但结果直接掉在地上（表现为界面永远转圈）。
    //   直连（同线程）：解析在槽里同步完成，避免跨线程拷贝自定义结构体。
    if (m_http) {
        connect(m_http, SIGNAL(finished(int,QString,nm::NmJson,nm::NmHttpResponse)),
                this, SLOT(onHttpFinished(int,QString,nm::NmJson,nm::NmHttpResponse)));
    }
}

void NmApi::setSession(NmSession *session)
{
    m_session = session;
}

QList<KeyValue> NmApi::commonHeaders() const
{
    QList<KeyValue> headers;
    headers.append(qMakePair(QString(QLatin1String("Referer")),
                             QString(QLatin1String("http://music.163.com/"))));
    // UA 由 NmHttpClient 默认注入，这里不重复
    if (m_session && m_session->hasCookie())
        headers.append(qMakePair(QString(QLatin1String("Cookie")),
                                 m_session->cookieHeader()));
    return headers;
}

int NmApi::sendGet(const QString &tag, const QString &path,
                   const QList<KeyValue> &params)
{
    if (!m_http)
        return -1;

    NmHttpRequest request;
    request.method = QByteArray("GET");
    request.url = QString(QLatin1String("%1%2")).arg(QLatin1String(kApiBase), path);
    request.params = params;
    request.headers = commonHeaders();
    request.tag = tag;
    return m_http->send(request);
}

int NmApi::searchSongs(const QString &keyword, int limit, int offset)
{
    QList<KeyValue> params;
    params.append(qMakePair(QString(QLatin1String("csrfToken")), QString()));
    params.append(qMakePair(QString(QLatin1String("hlpretag")),
                            QString(QLatin1String("<span class=\"s-fc2\">"))));
    params.append(qMakePair(QString(QLatin1String("hlposttag")),
                            QString(QLatin1String("</span>"))));
    params.append(qMakePair(QString(QLatin1String("s")), keyword));
    params.append(qMakePair(QString(QLatin1String("type")), QString::number(1)));
    params.append(qMakePair(QString(QLatin1String("offset")), QString::number(offset)));
    params.append(qMakePair(QString(QLatin1String("total")), QString(QLatin1String("true"))));
    params.append(qMakePair(QString(QLatin1String("limit")), QString::number(limit)));
    return sendGet(QLatin1String(tagSearch()), QLatin1String("/search/get/web"), params);
}

int NmApi::fetchPlaylist(const QString &playlistId)
{
    QList<KeyValue> params;
    params.append(qMakePair(QString(QLatin1String("id")), playlistId));
    params.append(qMakePair(QString(QLatin1String("t")), QString::number(0)));
    params.append(qMakePair(QString(QLatin1String("n")), QString::number(1000)));
    params.append(qMakePair(QString(QLatin1String("s")), QString::number(0)));
    return sendGet(QLatin1String(tagPlaylist()), QLatin1String("/v2/playlist/detail"), params);
}

int NmApi::fetchAlbum(qint64 albumId)
{
    const QString path = QString(QLatin1String("/v1/album/%1")).arg(albumId);
    return sendGet(QLatin1String(tagAlbum()), path, QList<KeyValue>());
}

int NmApi::fetchSongUrl(qint64 songId, int br)
{
    QList<KeyValue> params;
    params.append(qMakePair(QString(QLatin1String("br")), QString::number(br)));
    params.append(qMakePair(QString(QLatin1String("ids")),
                            encodeIdsParam(QList<qint64>() << songId)));

    const int requestId =
        sendGet(QLatin1String(tagSongUrl()), QLatin1String("/song/enhance/player/url"), params);
    if (requestId > 0)
        m_pendingSongIds.insert(requestId, songId);
    return requestId;
}

int NmApi::fetchAccount()
{
    QList<KeyValue> params;
    params.append(qMakePair(QString(QLatin1String("csrfToken")), QString()));
    return sendGet(QLatin1String(tagAccount()), QLatin1String("/nuser/account/get"), params);
}

int NmApi::fetchUserPlaylists(qint64 uid, int limit, int offset)
{
    QList<KeyValue> params;
    params.append(qMakePair(QString(QLatin1String("uid")), QString::number(uid)));
    params.append(qMakePair(QString(QLatin1String("limit")), QString::number(limit)));
    params.append(qMakePair(QString(QLatin1String("offset")), QString::number(offset)));
    params.append(qMakePair(QString(QLatin1String("csrfToken")), QString()));
    return sendGet(QLatin1String(tagUserPlaylists()), QLatin1String("/user/playlist"), params);
}

int NmApi::fetchSongDetail(const QList<qint64> &ids)
{
    if (ids.isEmpty())
        return -1;

    QList<KeyValue> params;
    params.append(qMakePair(QString(QLatin1String("ids")), encodeIdsParam(ids)));
    return sendGet(QLatin1String(tagSongDetail()), QLatin1String("/song/detail"), params);
}

int NmApi::fetchDiscoverPlaylists(int limit)
{
    QList<KeyValue> params;
    params.append(qMakePair(QString(QLatin1String("limit")), QString::number(limit)));
    params.append(qMakePair(QString(QLatin1String("offset")), QString::number(0)));
    params.append(qMakePair(QString(QLatin1String("total")), QString(QLatin1String("true"))));
    params.append(qMakePair(QString(QLatin1String("n")), QString::number(limit)));
    return sendGet(QLatin1String(tagDiscover()), QLatin1String("/personalized/playlist"), params);
}

int NmApi::fetchRecommendSongs()
{
    QList<KeyValue> params;
    params.append(qMakePair(QString(QLatin1String("total")), QString(QLatin1String("true"))));
    return sendGet(QLatin1String(tagRecommend()),
                   QLatin1String("/discovery/recommend/songs"), params);
}

int NmApi::fetchComments(qint64 songId, int limit, int offset)
{
    const QString path = QString(QLatin1String("/v1/resource/comments/R_SO_4_%1"))
                             .arg(QString::number(songId));

    QList<KeyValue> params;
    params.append(qMakePair(QString(QLatin1String("limit")), QString::number(limit)));
    params.append(qMakePair(QString(QLatin1String("offset")), QString::number(offset)));
    params.append(qMakePair(QString(QLatin1String("csrfToken")), QString()));
    return sendGet(QLatin1String(tagComments()), path, params);
}

int NmApi::fetchMvComments(qint64 mvId, int limit, int offset)
{
    const QString path = QString(QLatin1String("/v1/resource/comments/R_MV_5_%1"))
                             .arg(QString::number(mvId));

    QList<KeyValue> params;
    params.append(qMakePair(QString(QLatin1String("limit")), QString::number(limit)));
    params.append(qMakePair(QString(QLatin1String("offset")), QString::number(offset)));
    params.append(qMakePair(QString(QLatin1String("csrfToken")), QString()));
    return sendGet(QLatin1String(tagComments()), path, params);
}

int NmApi::fetchLyric(qint64 songId)
{
    /*
     * 歌词：老接口，os=pc 时返回结构最全。
     * ★ lv / kv / tv 传 -1：服务端一次把【原词 + 翻译】都给出来，
     *   不用为了翻译再发一次请求。
     */
    QList<KeyValue> params;
    params.append(qMakePair(QString(QLatin1String("os")),
                            QString(QLatin1String("pc"))));
    params.append(qMakePair(QString(QLatin1String("id")), QString::number(songId)));
    params.append(qMakePair(QString(QLatin1String("lv")), QString::number(-1)));
    params.append(qMakePair(QString(QLatin1String("kv")), QString::number(-1)));
    params.append(qMakePair(QString(QLatin1String("tv")), QString::number(-1)));

    return sendGet(QLatin1String(tagLyric()), QLatin1String("/song/lyric"), params);
}

int NmApi::fetchQrKey()
{
    /*
     * 扫码登录第一步。type=3 表示"生成给 PC/网页端用的登录二维码"
     * （照参考项目 cloudmusicqt 的取值）。
     */
    QList<KeyValue> params;
    params.append(qMakePair(QString(QLatin1String("type")),
                            QString::number(3)));
    return sendGet(QLatin1String(tagQrKey()),
                   QLatin1String("/login/qrcode/unikey"), params);
}

int NmApi::fetchQrStatus(const QString &key)
{
    QList<KeyValue> params;
    params.append(qMakePair(QString(QLatin1String("key")), key));
    params.append(qMakePair(QString(QLatin1String("type")),
                            QString::number(3)));
    return sendGet(QLatin1String(tagQrStatus()),
                   QLatin1String("/login/qrcode/client/login"), params);
}

int NmApi::fetchPlaylistComments(qint64 playlistId, int limit, int offset)
{
    const QString path = QString(QLatin1String("/v1/resource/comments/A_PL_0_%1"))
                             .arg(QString::number(playlistId));

    QList<KeyValue> params;
    params.append(qMakePair(QString(QLatin1String("limit")), QString::number(limit)));
    params.append(qMakePair(QString(QLatin1String("offset")), QString::number(offset)));
    params.append(qMakePair(QString(QLatin1String("csrfToken")), QString()));
    return sendGet(QLatin1String(tagComments()), path, params);
}

int NmApi::fetchUserLevel()
{
    QList<KeyValue> params;
    params.append(qMakePair(QString(QLatin1String("csrfToken")), QString()));
    return sendGet(QLatin1String(tagLevel()), QLatin1String("/user/level"), params);
}

int NmApi::fetchUserDetail(qint64 uid)
{
    // ★ 路径式：/api/v1/user/detail/<uid>（query 形式 user/detail?uid= 实测 404）
    QList<KeyValue> params;
    params.append(qMakePair(QString(QLatin1String("csrfToken")), QString()));
    return sendGet(QLatin1String(tagUserDetail()),
                   QLatin1String("/v1/user/detail/") + QString::number(uid),
                   params);
}

int NmApi::fetchSubscribedArtists(int limit, int offset)
{
    QList<KeyValue> params;
    params.append(qMakePair(QString(QLatin1String("limit")), QString::number(limit)));
    params.append(qMakePair(QString(QLatin1String("offset")), QString::number(offset)));
    return sendGet(QLatin1String(tagArtists()), QLatin1String("/artist/sublist"), params);
}

int NmApi::fetchMvList(const QString &kind, int limit, int offset)
{
    QList<KeyValue> params;
    params.append(qMakePair(QString(QLatin1String("limit")), QString::number(limit)));
    params.append(qMakePair(QString(QLatin1String("offset")), QString::number(offset)));

    const QString path = (kind == QLatin1String("first"))
                             ? QLatin1String("/mv/first")
                             : QLatin1String("/mv/all");
    return sendGet(QLatin1String(tagMvs()), path, params);
}

int NmApi::fetchMvUrl(qint64 mvId, int r)
{
    QList<KeyValue> params;
    params.append(qMakePair(QString(QLatin1String("id")), QString::number(mvId)));
    params.append(qMakePair(QString(QLatin1String("r")), QString::number(r)));
    return sendGet(QLatin1String(tagMvUrl()),
                   QLatin1String("/song/enhance/play/mv/url"), params);
}

int NmApi::fetchCloud(int limit, int offset)
{
    QList<KeyValue> params;
    params.append(qMakePair(QString(QLatin1String("limit")), QString::number(limit)));
    params.append(qMakePair(QString(QLatin1String("offset")), QString::number(offset)));
    return sendGet(QLatin1String(tagCloud()), QLatin1String("/v1/cloud/get"), params);
}

void NmApi::onHttpFinished(int requestId, const QString &tag,
                           const nm::NmJson &json,
                           const nm::NmHttpResponse &response)
{
    // 网络/HTTP 层失败（非 2xx、超时、断网、解压失败等）
    if (!response.ok) {
        QVariantMap detail;
        detail.insert(QLatin1String("httpStatus"), response.httpStatus);
        detail.insert(QLatin1String("networkError"), response.networkError);
        detail.insert(QLatin1String("url"), response.finalUrl);
        detail.insert(QLatin1String("tag"), response.tag);
        detail.insert(QLatin1String("raw"),
                      QString::fromUtf8(response.body.left(512)));
        emit requestFailed(requestId, tag, response.errorMessage, detail);
        return;
    }

    if (tag == QLatin1String(tagSearch())) {
        NmParsers::SearchParse result = NmParsers::parseSearch(json);
        emit searchFinished(requestId, result);
        return;
    }

    if (tag == QLatin1String(tagPlaylist())) {
        NmParsers::PlaylistParse result = NmParsers::parsePlaylist(json);
        emit playlistFinished(requestId, result);
        return;
    }

    if (tag == QLatin1String(tagAlbum())) {
        NmParsers::PlaylistParse result = NmParsers::parseAlbum(json);
        emit albumFinished(requestId, result);
        return;
    }

    if (tag == QLatin1String(tagSongUrl())) {
        const qint64 songId = m_pendingSongIds.value(requestId, 0);
        m_pendingSongIds.remove(requestId);
        NmParsers::SongUrlParse result = NmParsers::parseSongUrl(json);
        emit songUrlFinished(requestId, songId, result);
        return;
    }

    if (tag == QLatin1String(tagAccount())) {
        NmParsers::AccountParse result = NmParsers::parseAccount(json);
        emit accountFinished(requestId, result);
        return;
    }

    if (tag == QLatin1String(tagUserPlaylists())) {
        NmParsers::UserPlaylistsParse result = NmParsers::parseUserPlaylists(json);
        emit userPlaylistsFinished(requestId, result);
        return;
    }

    if (tag == QLatin1String(tagSongDetail())) {
        NmParsers::SongDetailParse result = NmParsers::parseSongDetail(json);
        emit songDetailFinished(requestId, result);
        return;
    }

    if (tag == QLatin1String(tagDiscover())) {
        NmParsers::DiscoverParse result = NmParsers::parseDiscover(json);
        emit discoverFinished(requestId, result);
        return;
    }

    if (tag == QLatin1String(tagRecommend())) {
        NmParsers::DiscoverParse result = NmParsers::parseRecommend(json);
        emit recommendFinished(requestId, result);
        return;
    }

    if (tag == QLatin1String(tagComments())) {
        NmParsers::CommentsParse result = NmParsers::parseComments(json);
        emit commentsFinished(requestId, result);
        return;
    }

    if (tag == QLatin1String(tagLevel())) {
        NmParsers::LevelParse result = NmParsers::parseLevel(json);
        emit levelFinished(requestId, result);
        return;
    }

    if (tag == QLatin1String(tagUserDetail())) {
        NmParsers::UserDetailParse result = NmParsers::parseUserDetail(json);
        emit userDetailFinished(requestId, result);
        return;
    }

    if (tag == QLatin1String(tagArtists())) {
        NmParsers::ArtistsParse result = NmParsers::parseArtists(json);
        emit artistsFinished(requestId, result);
        return;
    }

    if (tag == QLatin1String(tagMvs())) {
        NmParsers::MvsParse result = NmParsers::parseMvs(json);
        emit mvsFinished(requestId, result);
        return;
    }

    if (tag == QLatin1String(tagMvUrl())) {
        NmParsers::MvUrlParse result = NmParsers::parseMvUrl(json);
        emit mvUrlFinished(requestId, result);
        return;
    }

    if (tag == QLatin1String(tagCloud())) {
        NmParsers::CloudParse result = NmParsers::parseCloud(json);
        emit cloudFinished(requestId, result);
        return;
    }

    /*
     * ★★ 歌词分派 —— 之前【这一整个分支是缺的】！
     *   fetchLyric 发得出请求（日志里能看到 GET /api/song/lyric、也有 200 响应），
     *   但响应回来在这里按 tag 分派时匹配不到任何分支，就被静默丢掉了：
     *     lyricFinished 永不 emit → MusicController::onLyricFinished 永不调用
     *     → 歌词、翻译全部为空（副标题只有「歌名 - 作者」）。
     *   这是"翻译开关点了没反应"的真正根因。
     */
    if (tag == QLatin1String(tagLyric())) {
        NmParsers::LyricParse result = NmParsers::parseLyric(json);
        emit lyricFinished(requestId, result);
        return;
    }

    /*
     * 扫码登录的两个响应。
     * ★ 别忘了加分派 —— 歌词那次的教训：请求发得出去、200 也回来了，
     *   但这里匹配不到分支就被静默丢掉，表现为"点了没反应"。
     */
    if (tag == QLatin1String(tagQrKey())) {
        NmParsers::QrKeyParse result = NmParsers::parseQrKey(json);
        emit qrKeyFinished(requestId, result);
        return;
    }

    if (tag == QLatin1String(tagQrStatus())) {
        NmParsers::QrStatusParse result = NmParsers::parseQrStatus(json);
        emit qrStatusFinished(requestId, result);
        return;
    }
}

} // namespace nm
