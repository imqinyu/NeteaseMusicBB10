#include "util/NmImageCache.hpp"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
// 写成 QtGui/QImage：工程里只配了 QT += network，QtGui 的 include 路径
// 不一定被加进来，但 .../include/qt4 一定在，用全路径最稳。
#include <QtGui/QImage>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

namespace nm {

namespace {

/*!
 * 排队上限。
 *
 * ★ 为什么需要：一个歌单可能几百首歌（实测 650 首），不限流的话
 *   fillSongsModel 会一口气排几百个下载，队列和随之而来的模型刷新
 *   会把 UI 拖住（滑动明显掉帧）。排满就先不排，等 refreshImagePaths
 *   再补 —— 它每完成一张图就跑一遍，会持续把队列喂满。
 */
static const int kMaxQueue = 150;

/*!
 * 把本地绝对路径转成 file:// URL。
 *
 * ★ 必须这样转（BBTieba 真机实测）：
 *   直接把 "/accounts/1000/..." 交给 ImageView.imageSource 会被当成
 *   asset 相对路径去 /apps/<appid>/native/assets/ 下找，然后报
 *   "Did not find any asset" / "Image not found"。加 file:// 才按本地文件处理。
 */
QString toFileUrl(const QString &absPath)
{
    if (absPath.isEmpty())
        return absPath;
    if (absPath.contains(QLatin1String("://")))
        return absPath;

    // Unix/QNX 绝对路径：file:// + /a/b = file:///a/b（三斜杠是对的）
    if (absPath.startsWith(QLatin1Char('/')))
        return QLatin1String("file://") + absPath;

    // Windows 盘符路径（桌面测试环境走这里）
    if (absPath.length() >= 2 && absPath.at(1) == QLatin1Char(':'))
        return QLatin1String("file:///") + absPath;

    return absPath;
}

/*!
 * 规范化图片地址：去掉 ? 后面的加工参数。
 *
 * ★ 为什么（真机实测）：网易返回的封面常带
 *       ?imageView=1&thumbnail=800y800&enlarge=1|...
 *   这类参数在设备上会被 CDN 拒掉（status 400），而 PC 上却正常；
 *   去掉问号后面那段就能下（PC 实测 200 / 869KB）。
 *   我们本来就要缩到 160px，原始大图完全够用，所以一律取裸地址。
 */
QString normalize(const QString &url)
{
    if (!url.startsWith(QLatin1String("http")))
        return url;
    const int q = url.indexOf(QLatin1Char('?'));
    if (q > 0)
        return url.left(q);
    return url;
}

/*! 从 URL 猜一个后缀（只接受常见图片后缀，避免怪字符进文件名） */
QString suffixFor(const QString &url)
{
    const int dot = url.lastIndexOf(QLatin1Char('.'));
    if (dot < 0)
        return QLatin1String(".img");
    QString ext = url.mid(dot);
    const int q = ext.indexOf(QLatin1Char('?'));
    if (q >= 0)
        ext = ext.left(q);
    if (ext.compare(QLatin1String(".jpg"), Qt::CaseInsensitive) == 0
            || ext.compare(QLatin1String(".jpeg"), Qt::CaseInsensitive) == 0
            || ext.compare(QLatin1String(".png"), Qt::CaseInsensitive) == 0
            || ext.compare(QLatin1String(".webp"), Qt::CaseInsensitive) == 0
            || ext.compare(QLatin1String(".bmp"), Qt::CaseInsensitive) == 0)
        return ext.toLower();
    return QLatin1String(".img");
}

} // anonymous namespace

NmImageCache::NmImageCache(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
    , m_timeoutMs(30000)
    , m_maxConcurrent(2)
    , m_maxImageSize(160)
    , m_verbose(false)
    , m_wired(false)
{
    // 图片下载不需要 cookie jar
    m_manager->setCookieJar(0);

    // 字符串版 connect 运行时才解析签名，存下结果供测试断言
    m_wired = connect(m_manager, SIGNAL(finished(QNetworkReply *)),
                      this, SLOT(onFinished(QNetworkReply *)));
}

NmImageCache::~NmImageCache()
{
}

void NmImageCache::cancelQueued()
{
    if (m_queue.isEmpty())
        return;

    foreach (const QString &url, m_queue)
        m_requested.remove(url);
    m_queue.clear();

    if (m_verbose)
        qWarning("NmImageCache: queue cancelled (was %d)", 0);
}

int NmImageCache::clear()
{
    const QString dir = ensureCacheDir();
    QDir d(dir);
    int removed = 0;

    if (d.exists()) {
        const QFileInfoList files = d.entryInfoList(QDir::Files);
        foreach (const QFileInfo &info, files) {
            if (QFile::remove(info.absoluteFilePath()))
                ++removed;
        }
    }

    // 正在下载的也一并放弃，避免回到旧状态后路径错乱
    foreach (QNetworkReply *reply, m_active.values()) {
        if (reply) {
            reply->abort();
            reply->deleteLater();
        }
    }
    m_active.clear();
    m_queue.clear();
    m_requested.clear();
    m_done.clear();

    return removed;
}

qint64 NmImageCache::cacheSizeBytes() const
{
    const QString dir = ensureCacheDir();
    QDir d(dir);
    qint64 total = 0;

    if (d.exists()) {
        foreach (const QFileInfo &info, d.entryInfoList(QDir::Files))
            total += info.size();
    }
    return total;
}

void NmImageCache::setMaxConcurrent(int n)
{
    m_maxConcurrent = (n < 1) ? 1 : n;
    pumpQueue();
}

QString NmImageCache::ensureCacheDir() const
{
    QString base = m_cacheDir;
    /*
     * ★★ 必须用【持久目录】，不能用 QDir::tempPath()！
     *   临时目录会在进程退出/设备重启时被系统清掉 —— 真机表现就是
     *   "每次重新打开软件，缓存全没了（0 files / 0.0 MB）"。
     *   QDir::homePath() 在 BB10 上是应用自己的私有数据目录，跨启动保留。
     */
    if (base.isEmpty())
        base = QDir::homePath() + QLatin1String("/nm-img");
    QDir dir(base);
    if (!dir.exists())
        dir.mkpath(QLatin1String("."));
    m_effectiveDir = base;
    return base;
}

QString NmImageCache::fileNameFor(const QString &url, int maxImageSize)
{
    // MD5 当文件名：URL 里有 ? 和 % 之类，直接用不安全
    const QByteArray hash = QCryptographicHash::hash(
        url.toUtf8(), QCryptographicHash::Md5).toHex();

    // 缩放后的图统一存成 jpg，文件名带上尺寸 —— 改了 maxImageSize
    // 就不会误命中上一次的大图缓存
    if (maxImageSize > 0)
        return QString::fromLatin1(hash) + QLatin1String("-s")
               + QString::number(maxImageSize) + QLatin1String(".jpg");

    return QString::fromLatin1(hash) + suffixFor(url);
}

QString NmImageCache::pathFor(const QString &urlIn)
{
    // 统一用规范化地址（去 ? 加工参数），缓存 key 和下载地址都用它
    const QString url = normalize(urlIn);

    if (url.trimmed().isEmpty())
        return QString();

    // 本来就是本地路径直接透传
    if (url.startsWith(QLatin1String("asset://"))
            || url.startsWith(QLatin1String("file://"))
            || url.startsWith(QLatin1String("data:")))
        return url;

    if (!url.startsWith(QLatin1String("http://"))
            && !url.startsWith(QLatin1String("https://")))
        return QString();

    // 命中内存缓存：最热路径，不重算 MD5、不 stat 文件
    if (m_done.contains(url))
        return toFileUrl(m_done.value(url));

    // 之前下载失败的（如 404 的头像）：本次运行内不再重试
    if (m_failed.contains(url))
        return QString();

    // 已在下载 / 已排队：不要重复入队
    if (m_requested.contains(url))
        return QString();

    const QString dir = ensureCacheDir();
    if (dir.isEmpty()) {
        qWarning("NmImageCache: cache dir unavailable");
        return QString();
    }

    const QString path = dir + QLatin1Char('/') + fileNameFor(url, m_maxImageSize);

    // 上次运行留下的文件：直接复用，并进内存缓存
    if (QFileInfo(path).exists() && QFileInfo(path).size() > 0) {
        m_done.insert(url, path);
        return toFileUrl(path);
    }

    // 队列满了就先不排（等 refreshImagePaths 补，见 kMaxQueue 的说明）
    if (m_queue.size() >= kMaxQueue) {
        m_requested.remove(url);
        return QString();
    }

    m_requested.insert(url);
    m_queue.append(url);
    if (m_verbose)
        qWarning("NmImageCache: queued %s (queue=%d)", qPrintable(url.left(120)), m_queue.size());

    pumpQueue();
    return QString();
}

void NmImageCache::pumpQueue()
{
    while (!m_queue.isEmpty() && m_active.size() < m_maxConcurrent)
        startDownload(m_queue.takeFirst());
}

void NmImageCache::startDownload(const QString &url)
{
    const QString dir = ensureCacheDir();
    if (dir.isEmpty()) {
        m_requested.remove(url);
        return;
    }

    const QString path = dir + QLatin1Char('/') + fileNameFor(url, m_maxImageSize);

    // 排队期间可能已被写好，再确认一次
    if (QFileInfo(path).exists() && QFileInfo(path).size() > 0) {
        m_done.insert(url, path);
        m_requested.remove(url);
        emit ready();
        return;
    }

    QNetworkRequest request((QUrl(url)));
    request.setRawHeader("User-Agent",
                         "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
                         "AppleWebKit/537.36 (KHTML, like Gecko) "
                         "Chrome/120.0.0.0 Safari/537.36");
    request.setRawHeader("Accept", "image/*,*/*;q=0.8");
    request.setRawHeader("Referer", "http://music.163.com/");
    request.setRawHeader("Accept-Encoding", "identity");

    QNetworkReply *reply = m_manager->get(request);
    if (!reply) {
        qWarning("NmImageCache: get() returned null");
        m_requested.remove(url);
        return;
    }

    reply->setProperty("nmUrl", url);
    reply->setProperty("nmPath", path);
    m_active.insert(url, reply);

    if (m_verbose)
        qWarning("NmImageCache: downloading (parallel %d/%d) -> %s",
                 m_active.size(), m_maxConcurrent, qPrintable(path));

    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
            this, SLOT(onReplyError(QNetworkReply::NetworkError)));

    /*
     * SSL 错误：打日志后忽略。BB10 的根证书库较旧/缺中间证书，
     * 拉的是公开封面图（不带凭据），忽略才能显示。
     */
    connect(reply, SIGNAL(sslErrors(QList<QSslError>)),
            this, SLOT(onReplySslErrors(QList<QSslError>)));

    // 超时保护（abort() 不是槽，超时回到自己身上显式处理）
    QTimer *timer = new QTimer(this);
    timer->setSingleShot(true);
    timer->setProperty("nmUrl", url);
    connect(timer, SIGNAL(timeout()), this, SLOT(onTimeout()));
    connect(reply, SIGNAL(finished()), timer, SLOT(stop()));
    connect(reply, SIGNAL(finished()), timer, SLOT(deleteLater()));
    timer->start(m_timeoutMs);
}

void NmImageCache::onReplyError(QNetworkReply::NetworkError code)
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    const QString url = reply ? reply->property("nmUrl").toString() : QString();
    qWarning("NmImageCache: network error code=%d url=%s",
             (int)code, qPrintable(url.left(120)));
}

void NmImageCache::onReplySslErrors(const QList<QSslError> &errors)
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    const QString url = reply ? reply->property("nmUrl").toString() : QString();

    for (int i = 0; i < errors.size(); ++i) {
        qWarning("NmImageCache: SSL error(%d/%d) %s  url=%s",
                 i + 1, errors.size(),
                 qPrintable(errors.at(i).errorString()),
                 qPrintable(url.left(100)));
    }

    if (reply)
        reply->ignoreSslErrors();
}

bool NmImageCache::retryStripped(const QString &url)
{
    const int q = url.indexOf(QLatin1Char('?'));
    if (q <= 0)
        return false;

    const QString stripped = url.left(q);
    // 已经试过裸地址就别再试了（防无限重试）
    if (m_done.contains(stripped) || m_retried.contains(stripped))
        return false;

    m_retried.insert(stripped);
    m_requested.remove(url);
    m_requested.insert(stripped);
    m_queue.append(stripped);

    if (m_verbose)
        qWarning("NmImageCache: retry without query -> %s",
                 qPrintable(stripped.left(120)));
    return true;
}

void NmImageCache::onTimeout()
{
    QTimer *timer = qobject_cast<QTimer *>(sender());
    if (!timer)
        return;
    const QString url = timer->property("nmUrl").toString();
    timer->deleteLater();

    if (!url.isEmpty()) {
        QNetworkReply *reply = m_active.take(url);
        if (reply) {
            reply->abort();
            reply->deleteLater();
        }
        m_requested.remove(url);
        qWarning("NmImageCache: download timeout url=%s",
                 qPrintable(url.left(120)));
    }

    pumpQueue();
}

void NmImageCache::onFinished(QNetworkReply *reply)
{
    // reply 由 manager 的 finished(QNetworkReply*) 信号直接给出
    if (!reply) {
        qWarning("NmImageCache: finished with null reply");
        return;
    }

    const QString url = reply->property("nmUrl").toString();
    const QString path = reply->property("nmPath").toString();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QNetworkReply::NetworkError netError = reply->error();
    const QByteArray body = reply->readAll();
    const bool ok = (netError == QNetworkReply::NoError)
                    && (status == 200 || status == 0)
                    && !body.isEmpty();
    reply->deleteLater();

    m_active.remove(url);

    if (m_verbose) {
        qWarning("NmImageCache: finished status=%d err=%d bytes=%d (active %d, queued %d)",
                 status, (int)netError, body.size(), m_active.size(), m_queue.size());
    }

    if (!ok || path.isEmpty()) {
        qWarning("NmImageCache: download failed url=%s status=%d err=%d",
                 qPrintable(url.left(120)), status, (int)netError);

        // 带 ?... 加工参数的图可能被 CDN 拒：去掉参数再试一次
        if (retryStripped(url)) {
            pumpQueue();
            return;
        }

        // 记进失败名单：有些图（比如已删除的用户头像）会 404，
        // 每次列表刷新都重试一遍纯属浪费流量（真机日志里能看到同一条反复刷）
        m_failed.insert(url);
        m_requested.remove(url);
        pumpQueue();
        return;
    }

    /*
     * 落盘前缩小（见 setMaxImageSize 的说明）。
     * 解码失败（非图片内容/格式不支持）就直接写原始字节，
     * 保证功能不死 —— 最多是慢一点。
     */
    bool written = false;
    if (m_maxImageSize > 0) {
        QImage img;
        if (img.loadFromData(body)) {
            const QSize size = img.size();
            if (size.width() > m_maxImageSize || size.height() > m_maxImageSize) {
                img = img.scaled(m_maxImageSize, m_maxImageSize,
                                 Qt::KeepAspectRatio, Qt::FastTransformation);
            }
            written = img.save(path, "JPG", 80);
            if (m_verbose && written) {
                qWarning("NmImageCache: %dx%d -> %dx%d  %s",
                         size.width(), size.height(),
                         img.width(), img.height(), qPrintable(path));
            }
        }
    }

    if (!written) {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly)) {
            qWarning("NmImageCache: cannot write %s", qPrintable(path));
            m_requested.remove(url);
            pumpQueue();
            return;
        }
        f.write(body);
        f.close();
    }

    m_done.insert(url, path);
    emit ready();
    pumpQueue();
}

} // namespace nm
