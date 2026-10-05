/*
 * NmHttpClient - 基于 QNetworkAccessManager 的 HTTP 客户端
 *
 * Qt 4.8 的网络栈有几个坑，这里逐个绕开：
 *   1. 重定向：默认不跟随（QNetworkRequest::RedirectionTargetAttribute 要自己处理），
 *      这里手工跟随最多 5 跳；同时注意 Qt 4.8 的自动重定向策略会把 POST 变 GET，
 *      所以显式关掉并把重定向的 URL 交回上层。
 *   2. gzip：Qt 4.8 不自动解压，需要自己 inflate。BB10 上有 zlib（-lz），
 *      用 NM_HAVE_ZLIB 控制；未开启时不发 Accept-Encoding，避免服务端返回压缩内容。
 *   3. cookie：贴吧登录态主要靠 BDUSS 参数，cookie 只作补充，自己维护。
 *
 * 本类只负责「发出去、收回来」；公共参数与 sign 由 NmSigner / NmApi 负责。
 */

#ifndef NM_HTTP_CLIENT_HPP
#define NM_HTTP_CLIENT_HPP

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QObject>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVariant>   // Q_DECLARE_METATYPE 来自 qmetatype.h，必须显式引入

#include "util/NmJson.hpp"
#include "util/NmString.hpp"

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;

namespace nm {

class NmCookieStore;

/*! 一次请求的描述。值语义，可自由拷贝、排队 */
struct NmHttpRequest
{
    enum Method {
        Get,
        Post
    };

    QByteArray method;          // "GET" / "POST"
    QString url;
    QList<KeyValue> params;     // POST -> 表单体；GET -> 查询串
    QList<KeyValue> headers;
    QByteArray body;            // 当 params 为空且 body 非空时直接用 body（支持 multipart）
    QString tag;                // 上层用来识别是哪个业务请求
    int timeoutMs;

    NmHttpRequest()
        : method("POST")
        , timeoutMs(30000)
    {
    }

    bool isGet() const { return method == QByteArray("GET"); }
};

/*! 一次响应 */
struct NmHttpResponse
{
    bool ok;
    int httpStatus;
    QByteArray body;
    QString contentType;
    QString errorMessage;
    QString tag;
    QString finalUrl;           // 跟随重定向后的最终 URL
    int networkError;           // QNetworkReply::NetworkError

    NmHttpResponse()
        : ok(false)
        , httpStatus(0)
        , networkError(0)
    {
    }

    /*! 解析为 JSON；失败时通过 error 返回原因 */
    NmJson json(QString *error = 0) const;
};

class NmHttpClient : public QObject
{
    Q_OBJECT
public:
    explicit NmHttpClient(QObject *parent = 0);
    virtual ~NmHttpClient();

    /*! 必须在使用前注入 cookie 存储（不获取所有权） */
    void setCookieStore(NmCookieStore *store);

    /*! 发起请求。返回请求 id，可在调试面板里对应 */
    int send(const NmHttpRequest &request);

    /*! 调试开关：为真时记录请求/响应原文（真机排查接口用） */
    void setDebugEnabled(bool enabled);
    bool isDebugEnabled() const { return m_debug; }

    /*!
     * 控制台日志开关：为真时把每条接口日志直接打到 stderr
     * （IDE 的 console / 真机 slog2 里能实时看到），并顺带打开收集。
     */
    void setConsoleLogEnabled(bool enabled);
    bool isConsoleLogEnabled() const { return m_consoleLog; }

    /*! 最近的调试记录（最新在前） */
    QStringList debugLog() const { return m_debugLog; }
    void clearDebugLog();

    /*! 取消所有在途请求 */
    void abortAll();

    /*! 超时设置（毫秒），默认 30 秒 */
    void setTimeout(int ms) { m_timeoutMs = ms; }
    int timeout() const { return m_timeoutMs; }

    /*! 是否编译进了 gzip 支持 */
    static bool hasGzipSupport();

    /*!
     * 是否请求 gzip 压缩响应。
     *
     * 默认值 = 编译期是否有 zlib（NM_HAVE_ZLIB）。
     * 本机跑测试工具时通常没有 zlib，可以显式关掉，这样服务端就不会返回
     * 压缩内容，也就不需要解压。真机上有 zlib，保持默认即可。
     */
    void setAcceptGzip(bool enabled) { m_acceptGzip = enabled; }
    bool acceptGzip() const { return m_acceptGzip; }

signals:
    /*! 请求完成。req 里带上请求 id、业务 tag、解析好的 JSON */
    void finished(int requestId, const QString &tag, const nm::NmJson &json,
                  const nm::NmHttpResponse &response);

private slots:
    void onReplyFinished();
    void onReplyTimeout();
    void onReplyProgress(qint64 received, qint64 total);

private:
    QNetworkReply *dispatch(const NmHttpRequest &request, int requestId, int redirectCount);
    void handleReply(QNetworkReply *reply, const NmHttpRequest &request,
                     int requestId, int redirectCount);
    void emitFailure(int requestId, const QString &tag, const QString &url,
                     int status, int networkError, const QString &message,
                     const QByteArray &snippet);
    void appendDebug(const QString &line);

    static QString describeNetworkError(int error);
    static QString describeHttpStatus(int status);
    static QByteArray gzipInflate(const QByteArray &data, QString *error);
    static QByteArray sanitizeForLog(const QByteArray &body);

    /*!
     * 请求头与 QVariant 的互转。
     * Qt 4.8 的 QVariant 无法直接承载 QList<QPair<QString,QString>>，
     * 所以序列化成 "k: v\r\n" 文本再挂到 reply 属性上。
     */
    static QString headersToText(const QList<KeyValue> &headers);
    static QList<KeyValue> headersFromText(const QString &text);

    QNetworkAccessManager *m_manager;
    NmCookieStore *m_cookieStore;
    int m_nextRequestId;
    int m_timeoutMs;
    bool m_debug;
    /*! 控制台日志开关（见 setConsoleLogEnabled） */
    bool m_consoleLog;
    /*! 见 setAcceptGzip() 的说明 */
    bool m_acceptGzip;
    QStringList m_debugLog;
    QList<QNetworkReply *> m_inFlight;
    QList<int> m_timedOut;   // 被我们自己 abort 的请求 id
};

} // namespace nm

Q_DECLARE_METATYPE(nm::NmHttpResponse);

#endif // NM_HTTP_CLIENT_HPP
