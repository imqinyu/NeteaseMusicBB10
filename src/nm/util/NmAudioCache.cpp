#include "util/NmAudioCache.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QUrl>

namespace nm {

/*! 小于这个大小的文件当"残文件"，不算命中（下载中断会留下小碎片） */
static const qint64 kMinValidBytes = 8192;

NmAudioCache::NmAudioCache(QObject *parent)
    : QObject(parent)
    /*
     * ★★ 同 NmImageCache：必须用【持久目录】。
     *   QDir::tempPath() 会在进程退出时被清空，缓存根本活不过一次重启
     *   （真机反馈："重新打开程序发现缓存音乐丢了，audio: 0 files / 0.0 MB"）。
     */
    , m_cacheDir(QDir::homePath() + QLatin1String("/nm-audio"))
    , m_limitMb(1024)
    , m_nam(new QNetworkAccessManager(this))
{
    QDir().mkpath(m_cacheDir);

    QSettings settings;
    m_limitMb = settings.value(QLatin1String("nm/audioCacheMb"), 1024).toInt();
}

NmAudioCache::~NmAudioCache()
{
    // 退出时把还在下的请求掐掉（文件没写完，下次当没缓存就行）
    m_nam->deleteLater();
}

QString NmAudioCache::fileFor(qint64 songId) const
{
    return m_cacheDir + QLatin1Char('/') + QString::number(songId)
           + QLatin1String(".mp3");
}

QString NmAudioCache::pathFor(qint64 songId) const
{
    if (songId <= 0)
        return QString();

    const QString path = fileFor(songId);
    const QFileInfo info(path);
    if (!info.exists() || info.size() < kMinValidBytes)
        return QString();

    return QLatin1String("file://") + path;
}

void NmAudioCache::download(qint64 songId, const QString &url)
{
    if (songId <= 0 || url.isEmpty())
        return;
    if (url.startsWith(QLatin1String("file://")))
        return;                                   // 已经是本地文件，不用再下
    if (!pathFor(songId).isEmpty())
        return;                                   // 已缓存
    if (m_activeIds.contains(songId))
        return;                                   // 正在下

    const QUrl u(url);
    QNetworkRequest req(u);
    req.setRawHeader("User-Agent",
                     "Mozilla/5.0 (BB10; Touch) NeteaseMusic");

    QNetworkReply *reply = m_nam->get(req);
    reply->setProperty("nmSongId", (qlonglong)songId);
    m_activeIds.insert(songId);

    connect(reply, SIGNAL(finished()), this, SLOT(onFinished()));
}

void NmAudioCache::onFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply)
        return;

    const qint64 songId = reply->property("nmSongId").toLongLong();
    m_activeIds.remove(songId);

    const bool ok = (reply->error() == QNetworkReply::NoError);
    const QByteArray data = reply->readAll();
    reply->deleteLater();

    if (!ok || data.size() < kMinValidBytes) {
        /*
         * ★ 失败 / 半途断流：把可能的残文件删掉。
         *   留着的话，下次 pathFor() 会因为"文件太小"放过它，还算安全；
         *   但删干净更省事。
         */
        QFile::remove(fileFor(songId));
        return;
    }

    QFile f(fileFor(songId));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    f.write(data);
    f.close();

    enforceLimit();
}

void NmAudioCache::enforceLimit()
{
    if (m_limitMb <= 0)
        return;                                   // 0 = 不限

    QDir dir(m_cacheDir);
    // QDir::Time → 按修改时间【新 → 旧】排
    const QFileInfoList files = dir.entryInfoList(
                QStringList() << QLatin1String("*.mp3"),
                QDir::Files, QDir::Time);

    qint64 total = 0;
    for (int i = 0; i < files.size(); ++i)
        total += files.at(i).size();

    const qint64 limit = (qint64)m_limitMb * 1024LL * 1024LL;

    // 从最旧的一头删起，降到上限以内就停
    for (int i = files.size() - 1; i >= 0 && total > limit; --i) {
        total -= files.at(i).size();
        QFile::remove(files.at(i).absoluteFilePath());
    }
}

int NmAudioCache::clear()
{
    QDir dir(m_cacheDir);
    const QStringList names = dir.entryList(
                QStringList() << QLatin1String("*.mp3"), QDir::Files);

    int removed = 0;
    foreach (const QString &name, names) {
        if (QFile::remove(m_cacheDir + QLatin1Char('/') + name))
            ++removed;
    }
    return removed;
}

qint64 NmAudioCache::cacheSizeBytes() const
{
    QDir dir(m_cacheDir);
    const QFileInfoList files = dir.entryInfoList(
                QStringList() << QLatin1String("*.mp3"), QDir::Files);

    qint64 total = 0;
    for (int i = 0; i < files.size(); ++i)
        total += files.at(i).size();
    return total;
}

int NmAudioCache::fileCount() const
{
    QDir dir(m_cacheDir);
    return dir.entryList(QStringList() << QLatin1String("*.mp3"),
                         QDir::Files).size();
}

void NmAudioCache::setLimitMb(int mb)
{
    if (mb < 0)
        mb = 0;
    if (m_limitMb == mb)
        return;
    m_limitMb = mb;

    QSettings settings;
    settings.setValue(QLatin1String("nm/audioCacheMb"), m_limitMb);

    // 立刻按新上限清一次（用户把上限调小了往往就是想马上腾空间）
    enforceLimit();
}

} // namespace nm
