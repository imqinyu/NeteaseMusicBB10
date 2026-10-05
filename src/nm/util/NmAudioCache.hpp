/*
 * NmAudioCache - 播放过的歌曲音频落盘缓存（方案 A：先播、后台悄悄缓存）
 *
 * ── 为什么这么设计 ──────────────────────────────────────────────────────
 *
 * BB10 的 MediaPlayer 可以直接流式播 HTTP URL（现在的做法）。想省流量就得
 * 先把音频落到本地文件，但"先下完再播"会让每次点歌都要等一会儿 ——
 * 体验优先，所以这里走【播放的同时悄悄缓存】：
 *
 *   pathFor(id) 命中 → 直接用本地文件播，一个字节都不耗；
 *   没命中        → 照常流式播（体验完全不变），同时 download() 后台下，
 *                   下次再听同一首就是本地文件了。
 *
 * 目录用 QDir::tempPath() 下的子目录（和图片缓存同一个思路），跨启动复用；
 * 按上限（MB）自动清理最旧的文件，上限在设置页可调，0 = 不限。
 */

#ifndef NM_AUDIO_CACHE_HPP
#define NM_AUDIO_CACHE_HPP

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

namespace nm {

class NmAudioCache : public QObject
{
    Q_OBJECT
public:
    explicit NmAudioCache(QObject *parent = 0);
    virtual ~NmAudioCache();

    /*!
     * 命中返回 file:// 本地路径；没有则返回空串。
     * ★ 只查、不下载 —— 调用方拿空串就继续用网络地址播。
     */
    QString pathFor(qint64 songId) const;

    /*! 后台缓存这首歌（播放的同时悄悄下；已缓存 / 正在下则直接跳过） */
    void download(qint64 songId, const QString &url);

    /*! 清空音频缓存，@return 删掉的文件数（设置页的「清除音乐缓存」用） */
    int clear();

    /*! 缓存目录里的字节数 / 文件数（设置页显示用） */
    qint64 cacheSizeBytes() const;
    int fileCount() const;

    /*! 上限（MB）。0 = 不限。超了按"最旧优先"删。写 QSettings 持久化。 */
    void setLimitMb(int mb);
    int limitMb() const { return m_limitMb; }

    QString cacheDir() const { return m_cacheDir; }

private slots:
    void onFinished();

private:
    QString fileFor(qint64 songId) const;
    /*! 超出上限就删最旧的文件，直到降下来 */
    void enforceLimit();

    QString m_cacheDir;
    int m_limitMb;
    QNetworkAccessManager *m_nam;
    /*! 正在下载的歌曲 id（防重复下同一首） */
    QSet<qint64> m_activeIds;
};

} // namespace nm

#endif
