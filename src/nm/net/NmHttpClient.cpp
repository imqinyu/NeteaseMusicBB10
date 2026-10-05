#include "NmHttpClient.hpp"

#include "net/NmCookieStore.hpp"
#include "util/NmTr.hpp"

#include <QDateTime>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#include <QVariant>
#include <QtGlobal>

#include <stdio.h>
#include <string.h>

#if defined(NM_HAVE_ZLIB)
#include <zlib.h>
#endif

namespace nm {

namespace {

const char kDefaultUserAgent[] =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0 Safari/537.36";
const int kMaxRedirects = 5;

/*! 请求头里找某个键（不区分大小写） */
QString headerValue(const QList<KeyValue> &headers, const QString &name)
{
    for (int i = 0; i < headers.size(); ++i) {
        if (headers.at(i).first.compare(name, Qt::CaseInsensitive) == 0)
            return headers.at(i).second;
    }
    return QString();
}

} // anonymous namespace

NmHttpClient::NmHttpClient(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
    , m_cookieStore(0)
    , m_nextRequestId(1)
    , m_timeoutMs(30000)
    , m_debug(false)
    , m_consoleLog(false)
    , m_acceptGzip(hasGzipSupport())
{
    // 显式清空 cookie jar：登录态由我们自己的 Cookie 头承载
    // （MUSIC_U 等），用 Qt 自己的 jar 反而会把显式设置的
    // Cookie 头覆盖掉。
    m_manager->setCookieJar(0);

    qRegisterMetaType<nm::NmHttpResponse>("nm::NmHttpResponse");
}

NmHttpClient::~NmHttpClient()
{
}

void NmHttpClient::setCookieStore(NmCookieStore *store)
{
    m_cookieStore = store;
}

bool NmHttpClient::hasGzipSupport()
{
#if defined(NM_HAVE_ZLIB)
    return true;
#else
    return false;
#endif
}

void NmHttpClient::appendDebug(const QString &line)
{
    if (!m_debug)
        return;
    const QString stamped = QString(QLatin1String("[%1] %2"))
                                .arg(QDateTime::currentDateTime().toString(
                                         QLatin1String("HH:mm:ss.zzz")))
                                .arg(line);
    // 控制台日志：直接打 stderr（IDE console / 真机 slog2 实时可见）
    if (m_consoleLog)
        qWarning("[nm] %s", qPrintable(stamped));
    m_debugLog.prepend(stamped);
    // 只留最近 200 条
    while (m_debugLog.size() > 200)
        m_debugLog.removeLast();
}

void NmHttpClient::clearDebugLog()
{
    m_debugLog.clear();
}

void NmHttpClient::setDebugEnabled(bool enabled)
{
    m_debug = enabled;
}

void NmHttpClient::setConsoleLogEnabled(bool enabled)
{
    m_consoleLog = enabled;
    // 要往控制台打，就得先把日志收集打开
    m_debug = enabled;
}

QByteArray NmHttpClient::sanitizeForLog(const QByteArray &body)
{
    // 日志里不能泄露登录凭证（MUSIC_U）与签名
    QByteArray out = body;
    const char *sensitive[] = { "MUSIC_U=", "BDUSS=", "stoken=", "sign=" };
    for (unsigned i = 0; i < sizeof(sensitive) / sizeof(sensitive[0]); ++i) {
        const QByteArray needle(sensitive[i]);
        int from = 0;
        while (true) {
            const int at = out.indexOf(needle, from);
            if (at < 0)
                break;
            int end = out.indexOf('&', at);
            if (end < 0)
                end = out.size();
            out.replace(at + needle.size(), end - at - needle.size(),
                        QByteArray("***"));
            from = at + needle.size() + 3;
        }
    }
    return out;
}

QString NmHttpClient::describeNetworkError(int error)
{
    switch (error) {
    case QNetworkReply::ConnectionRefusedError:
        return NmTr("E8BF9EE68EA5E8A2ABE68B92E7BB9D");
    case QNetworkReply::RemoteHostClosedError:
        return NmTr("E8BF9EE68EA5E8A2ABE5AFB9E696B9E585B3E997AD");
    case QNetworkReply::HostNotFoundError:
        return NmTr("E689BEE4B88DE588B0E69C8DE58AA1E599A828444E5320E8A7A3E69E90E5A4B1E8B4A529");
    case QNetworkReply::TimeoutError:
        return NmTr("E8BF9EE68EA5E8B685E697B6");
    case QNetworkReply::OperationCanceledError:
        return NmTr("E8AFB7E6B182E8A2ABE58F96E6B688");
    case QNetworkReply::SslHandshakeFailedError:
        return NmTr("53534C20E68FA1E6898BE5A4B1E8B4A5");
    case QNetworkReply::TemporaryNetworkFailureError:
        return NmTr("E7BD91E7BB9CE4B8B4E697B6E69585E99A9C");
    case QNetworkReply::UnknownNetworkError:
        return NmTr("E69CAAE79FA5E7BD91E7BB9CE99499E8AFAF");
    default:
        return NmTr("E7BD91E7BB9CE99499E8AFAF28E4BBA3E7A08120253129").arg(error);
    }
}

QString NmHttpClient::describeHttpStatus(int status)
{
    switch (status) {
    case 301: case 302: case 303: case 307: case 308:
        return NmTr("E9878DE5AE9AE59091E6ACA1E695B0E8BF87E5A49A");
    case 403:
        return NmTr("E69C8DE58AA1E599A8E68B92E7BB9DE8AEBFE997AE283430332CE9809AE5B8B8E698AFE68EA5E58FA3E989B4E69D83E68896207369676E20E6A0A1E9AA8CE5A4B1E8B4A529");
    case 404:
        return NmTr("E68EA5E58FA3E4B88DE5AD98E59CA8283430342CE58FAFE883BDE68EA5E58FA3E8B7AFE5BE84E5B7B2E58F98E69BB429");
    case 408:
        return NmTr("E8AFB7E6B182E8B685E697B62834303829");
    case 429:
        return NmTr("E8AFB7E6B182E8BF87E4BA8EE9A291E7B9812834323929");
    case 500: case 502: case 503: case 504:
        return NmTr("E69C8DE58AA1E7ABAFE99499E8AFAF28253129").arg(status);
    default:
        return NmTr("4854545020E78AB6E68081E7A0812531").arg(status);
    }
}

QByteArray NmHttpClient::gzipInflate(const QByteArray &data, QString *error)
{
#if defined(NM_HAVE_ZLIB)
    if (data.isEmpty()) {
        if (error)
            *error = NmTr("E58E8BE7BCA9E695B0E68DAEE4B8BAE7A9BA");
        return QByteArray();
    }

    z_stream stream;
    memset(&stream, 0, sizeof(stream));

    // 16 + MAX_WBITS：接受 gzip 与 zlib 两种包装
    if (inflateInit2(&stream, 16 + MAX_WBITS) != Z_OK) {
        if (error)
            *error = NmTr("7A6C696220E5889DE5A78BE58C96E5A4B1E8B4A5");
        return QByteArray();
    }

    QByteArray out;
    char buffer[32768];
    stream.next_in = (Bytef *)data.constData();
    stream.avail_in = (uInt)data.size();

    int result = Z_OK;
    do {
        stream.next_out = (Bytef *)buffer;
        stream.avail_out = (uInt)sizeof(buffer);
        result = inflate(&stream, Z_NO_FLUSH);
        if (result != Z_OK && result != Z_STREAM_END && result != Z_BUF_ERROR) {
            inflateEnd(&stream);
            if (error)
                *error = NmTr("E8A7A3E58E8BE5A4B1E8B4A5287A6C696220E4BBA3E7A08120253129").arg(result);
            return QByteArray();
        }
        const int produced = (int)(sizeof(buffer) - stream.avail_out);
        if (produced > 0)
            out.append(buffer, produced);
        if (result == Z_BUF_ERROR && stream.avail_in == 0)
            break;
    } while (result != Z_STREAM_END);

    inflateEnd(&stream);
    return out;
#else
    Q_UNUSED(data)
    if (error)
        *error = NmTr("E69CACE6ACA1E69E84E5BBBAE69CAAE590AFE794A820677A697020E694AFE68C812854425F484156455F5A4C494229");
    return QByteArray();
#endif
}

QString NmHttpClient::headersToText(const QList<KeyValue> &headers)
{
    QStringList lines;
    for (int i = 0; i < headers.size(); ++i) {
        lines.append(headers.at(i).first + QLatin1String(": ") + headers.at(i).second);
    }
    return lines.join(QLatin1String("\r\n"));
}

QList<KeyValue> NmHttpClient::headersFromText(const QString &text)
{
    QList<KeyValue> out;
    if (text.isEmpty())
        return out;
    const QStringList lines = text.split(QLatin1String("\r\n"), QString::SkipEmptyParts);
    for (int i = 0; i < lines.size(); ++i) {
        const int colon = lines.at(i).indexOf(QLatin1String(": "));
        if (colon <= 0)
            continue;
        out.append(qMakePair(lines.at(i).left(colon), lines.at(i).mid(colon + 2)));
    }
    return out;
}

int NmHttpClient::send(const NmHttpRequest &request)
{
    const int requestId = m_nextRequestId++;

    if (request.url.isEmpty()) {
        emitFailure(requestId, request.tag, request.url, 0, 0,
                    NmTr("E8AFB7E6B1822055524C20E4B8BAE7A9BA"), QByteArray());
        return requestId;
    }

    appendDebug(QString(QLatin1String(">>> #%1 %2 %3"))
                    .arg(requestId)
                    .arg(QString::fromLatin1(request.method.constData()))
                    .arg(request.url));

    QNetworkReply *reply = dispatch(request, requestId, 0);
    if (reply)
        handleReply(reply, request, requestId, 0);
    return requestId;
}

QNetworkReply *NmHttpClient::dispatch(const NmHttpRequest &request, int requestId,
                                      int redirectCount)
{
    Q_UNUSED(redirectCount)

    QUrl url(request.url);

    QByteArray body = request.body;
    if (request.isGet()) {
        // Qt 4.8 只有 encodedQuery()/setEncodedQuery()，没有 Qt5 的 query()/setQuery()
        const QString extra = NmString::buildQuery(request.params);
        if (!extra.isEmpty()) {
            QString query = url.encodedQuery();
            query = query.isEmpty() ? extra : (query + QLatin1Char('&') + extra);
            url.setEncodedQuery(query.toUtf8());
        }
    } else if (body.isEmpty() && !request.params.isEmpty()) {
        body = NmString::buildFormBody(request.params);
    }

    QNetworkRequest netRequest(url);
    netRequest.setRawHeader("Accept", "*/*");
    netRequest.setRawHeader("Accept-Language", "zh-CN,zh;q=0.9");

    if (hasGzipSupport() && m_acceptGzip)
        netRequest.setRawHeader("Accept-Encoding", "gzip");

    // User-Agent：调用方优先
    const QString ua = headerValue(request.headers, QLatin1String("User-Agent"));
    netRequest.setRawHeader("User-Agent",
                            (ua.isEmpty() ? QLatin1String(kDefaultUserAgent) : ua).toUtf8());

    if (!request.isGet() && !body.isEmpty())
        netRequest.setHeader(QNetworkRequest::ContentTypeHeader,
                             QLatin1String("application/x-www-form-urlencoded"));

    // 自定义请求头（含 Cookie / force_login 等）
    bool cookieProvided = false;
    for (int i = 0; i < request.headers.size(); ++i) {
        const QString &name = request.headers.at(i).first;
        const QString &value = request.headers.at(i).second;
        if (name.compare(QLatin1String("User-Agent"), Qt::CaseInsensitive) == 0)
            continue;
        if (name.compare(QLatin1String("Cookie"), Qt::CaseInsensitive) == 0)
            cookieProvided = true;
        netRequest.setRawHeader(name.toUtf8(), value.toUtf8());
    }

    // 没显式给 Cookie 时，用 cookie 存储补上
    if (!cookieProvided && m_cookieStore) {
        const QString cookie = m_cookieStore->cookieHeaderFor(request.url);
        if (!cookie.isEmpty())
            netRequest.setRawHeader("Cookie", cookie.toUtf8());
    }

    appendDebug(QString(QLatin1String("    #%1 body=%2"))
                    .arg(requestId)
                    .arg(QString::fromLatin1(sanitizeForLog(body).constData())));

    if (request.isGet())
        return m_manager->get(netRequest);
    return m_manager->post(netRequest, body);
}

void NmHttpClient::handleReply(QNetworkReply *reply, const NmHttpRequest &request,
                               int requestId, int redirectCount)
{
    // 把上下文挂在 reply 上，槽里再取（避免用 sender 的 map 查找）
    reply->setProperty("nm_request_id", requestId);
    reply->setProperty("nm_redirect_count", redirectCount);
    reply->setProperty("nm_tag", request.tag);
    reply->setProperty("nm_request_headers", headersToText(request.headers));
    reply->setProperty("nm_method", request.method);

    // 调试探针：stderr 直接进 Momentics 调试控制台（Cascades 的
    // qWarning/console.log 只进 slog2，那里看不到）。排查网络问题用。
    fprintf(stderr, "[NM] >>> #%d %s %s\n", requestId,
            request.method.constData(), request.url.toUtf8().constData());
    for (int i = 0; i < request.headers.size(); ++i) {
        QString value = request.headers.at(i).second;
        // MUSIC_U 打码（只看得到前缀，确认有没有带上即可）
        const int at = value.indexOf(QLatin1String("MUSIC_U="));
        if (at >= 0 && value.size() > at + 20)
            value = value.left(at + 20) + QLatin1String("...(masked)");
        fprintf(stderr, "[NM]     %s: %s\n",
                request.headers.at(i).first.toUtf8().constData(),
                value.toUtf8().constData());
    }
    fflush(stderr);

    m_inFlight.append(reply);

    // 超时：Qt 4.8 的 setTransferTimeout 不存在，自己做定时器
    QTimer *timer = new QTimer(reply);
    timer->setSingleShot(true);
    timer->setInterval(m_timeoutMs > 0 ? m_timeoutMs : 30000);
    timer->setProperty("nm_request_id", requestId);
    connect(timer, SIGNAL(timeout()), this, SLOT(onReplyTimeout()));
    timer->start();

    connect(reply, SIGNAL(finished()), this, SLOT(onReplyFinished()));
    connect(reply, SIGNAL(downloadProgress(qint64,qint64)),
            this, SLOT(onReplyProgress(qint64,qint64)));
}

void NmHttpClient::onReplyTimeout()
{
    QTimer *timer = qobject_cast<QTimer *>(sender());
    if (!timer)
        return;
    const int requestId = timer->property("nm_request_id").toInt();

    for (int i = 0; i < m_inFlight.size(); ++i) {
        QNetworkReply *reply = m_inFlight.at(i);
        if (reply && reply->property("nm_request_id").toInt() == requestId) {
            fprintf(stderr, "[NM] !!! #%d TIMEOUT after %d ms\n",
                    requestId, m_timeoutMs);
            fflush(stderr);
            appendDebug(NmTr("2121212023253120E8B685E697B62CE4B8BBE58AA8E4B8ADE6ADA2").arg(requestId));
            m_timedOut.append(requestId);
            reply->abort();
            return;
        }
    }
}

void NmHttpClient::onReplyProgress(qint64 received, qint64 total)
{
    // 收到第一个字节就说明连接活着，重置超时（避免大图下载被误杀）
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply)
        return;
    QTimer *timer = reply->findChild<QTimer *>();
    if (timer && timer->isActive())
        timer->start();
    Q_UNUSED(received)
    Q_UNUSED(total)
}

void NmHttpClient::onReplyFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply)
        return;

    // ---- 重要：先把需要的信息全部取出来，再让 reply 进入销毁队列 ----
    const int requestId = reply->property("nm_request_id").toInt();
    const int redirectCount = reply->property("nm_redirect_count").toInt();
    const QString tag = reply->property("nm_tag").toString();
    const QList<KeyValue> requestHeaders =
        headersFromText(reply->property("nm_request_headers").toString());
    const QByteArray requestMethod = reply->property("nm_method").toByteArray();
    const QUrl replyUrl = reply->url();

    const QVariant statusVariant =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    const int status = statusVariant.isValid() ? statusVariant.toInt() : 0;
    const QNetworkReply::NetworkError netError = reply->error();
    const QVariant redirectVariant =
        reply->attribute(QNetworkRequest::RedirectionTargetAttribute);

    // 收 Cookie
    if (m_cookieStore) {
        const QList<QByteArray> rawCookies = reply->rawHeaderList();
        for (int i = 0; i < rawCookies.size(); ++i) {
            if (rawCookies.at(i).toLower() == QByteArray("set-cookie")) {
                const QByteArray value = reply->rawHeader(rawCookies.at(i));
                m_cookieStore->setFromHeader(QString::fromLatin1(value.constData()),
                                             replyUrl.host());
            }
        }
    }

    QByteArray body = reply->readAll();

    // 调试探针：stderr 直接进 Momentics 调试控制台。
    // status=0 且 err!=0 说明 TCP/DNS 层就失败了。
    fprintf(stderr, "[NM] <<< #%d status=%d err=%d bytes=%d url=%s\n",
            requestId, status, (int)netError, body.size(),
            replyUrl.toString().toUtf8().constData());
    fflush(stderr);

    const QByteArray contentEncoding =
        reply->rawHeader("Content-Encoding").toLower().trimmed();

    reply->deleteLater();
    m_inFlight.removeAll(reply);

    // ---- 超时导致的 abort 单独归类 ----
    if (m_timedOut.removeAll(requestId) > 0) {
        emitFailure(requestId, tag, replyUrl.toString(), status, (int)netError,
                    NmTr("E8AFB7E6B182E8B685E697B628253120E6AFABE7A79229").arg(m_timeoutMs),
                    sanitizeForLog(body.left(512)));
        return;
    }

    // ---- 重定向：手工跟随（保持原请求头） ----
    if (status >= 300 && status < 400 && redirectVariant.isValid()) {
        if (redirectCount >= kMaxRedirects) {
            emitFailure(requestId, tag, replyUrl.toString(), status, (int)netError,
                        describeHttpStatus(status), QByteArray());
            return;
        }

        QUrl target = redirectVariant.toUrl();
        if (target.isRelative())
            target = replyUrl.resolved(target);

        appendDebug(QString(QLatin1String("<<< #%1 %2 redirect -> %3"))
                        .arg(requestId).arg(status).arg(target.toString()));

        NmHttpRequest next;
        next.url = target.toString();
        next.tag = tag;
        next.method = "GET";        // 重定向一律按 GET 重新取
        next.timeoutMs = m_timeoutMs;
        next.headers = requestHeaders;

        QNetworkReply *nextReply = dispatch(next, requestId, redirectCount + 1);
        if (nextReply)
            handleReply(nextReply, next, requestId, redirectCount + 1);
        return;
    }

    // ---- 解压 ----
    if (contentEncoding == QByteArray("gzip")
            || contentEncoding == QByteArray("deflate")) {
        QString gzipError;
        const QByteArray inflated = gzipInflate(body, &gzipError);
        if (inflated.isEmpty() && !gzipError.isEmpty()) {
            emitFailure(requestId, tag, replyUrl.toString(), status, (int)netError,
                        NmTr("E5938DE5BA94E8A7A3E58E8BE5A4B1E8B4A53A2531").arg(gzipError),
                        sanitizeForLog(body.left(512)));
            return;
        }
        body = inflated;
    }

    const QString contentType =
        QString::fromLatin1(reply->rawHeader("Content-Type").constData());

    appendDebug(QString(QLatin1String("<<< #%1 status=%2 bytes=%3 ctype=%4"))
                    .arg(requestId).arg(status).arg(body.size()).arg(contentType));

    // ---- 网络层错误 ----
    if (netError != QNetworkReply::NoError && status == 0) {
        emitFailure(requestId, tag, replyUrl.toString(), status, (int)netError,
                    describeNetworkError((int)netError),
                    sanitizeForLog(body.left(512)));
        return;
    }

    // ---- HTTP 层错误 ----
    if (status < 200 || status >= 300) {
        emitFailure(requestId, tag, replyUrl.toString(), status, (int)netError,
                    describeHttpStatus(status), sanitizeForLog(body.left(512)));
        return;
    }

    // ---- 成功 ----
    NmHttpResponse response;
    response.ok = true;
    response.httpStatus = status;
    response.body = body;
    response.contentType = contentType;
    response.tag = tag;
    response.finalUrl = replyUrl.toString();
    response.networkError = (int)netError;

    const NmJson json = NmJson::parse(body);
    if (!json.isValid()) {
        // 调试探针：原样打印响应体（UTF-8 中文直接可读），用于判断
        // 是 gzip 没解开、空响应、风控 HTML、还是重复拼接的 JSON。
        fprintf(stderr, "[NM] xxx #%d JSON fail: %s | enc=%s ctype=%s\nbody[raw, %d bytes]=%s\n",
                requestId, json.errorMessage().toUtf8().constData(),
                contentEncoding.constData(), contentType.toUtf8().constData(),
                body.size(), body.left(400).constData());
        fflush(stderr);

        appendDebug(NmTr("20202020232531204A534F4E20E8A7A3E69E90E5A4B1E8B4A53A202532")
                        .arg(requestId).arg(json.errorMessage()));
    }

    emit finished(requestId, tag, json, response);
}

void NmHttpClient::emitFailure(int requestId, const QString &tag, const QString &url,
                               int status, int networkError, const QString &message,
                               const QByteArray &snippet)
{
    NmHttpResponse response;
    response.ok = false;
    response.httpStatus = status;
    response.networkError = networkError;
    response.errorMessage = message;
    response.tag = tag;
    response.body = snippet;
    response.finalUrl = url;

    appendDebug(QString(QLatin1String("!!! #%1 %2 (%3) body=%4"))
                    .arg(requestId)
                    .arg(message)
                    .arg(status)
                    .arg(QString::fromLatin1(snippet.constData())));

    emit finished(requestId, tag, NmJson(), response);
}

void NmHttpClient::abortAll()
{
    const QList<QNetworkReply *> snapshot = m_inFlight;
    for (int i = 0; i < snapshot.size(); ++i) {
        if (snapshot.at(i))
            snapshot.at(i)->abort();
    }
    m_inFlight.clear();
}

NmJson NmHttpResponse::json(QString *error) const
{
    const NmJson parsed = NmJson::parse(body);
    if (error) {
        if (parsed.isValid())
            error->clear();
        else
            *error = parsed.errorMessage();
    }
    return parsed;
}

} // namespace nm
