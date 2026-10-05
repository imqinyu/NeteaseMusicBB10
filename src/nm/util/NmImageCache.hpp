/*
 * NmImageCache - 把网络图片（专辑封面等）下载到本地并缓存
 *
 * ── 为什么必须有这个东西 ──────────────────────────────────────────────────
 *
 * BBTieba 真机日志实测（本项目沿用同一结论）：
 *     "Unsupported scheme (http)  used in url (...). Image loading aborted."
 *     "Unsupported scheme (https) used in url (...). Image loading aborted."
 *
 * BB10 的 Cascades 图片加载器不支持任何网络方案，
 * `ImageView.imageSource` 只能吃 asset:// / file:// 这类本地路径。
 * 所以封面必须在 C++ 里下载成文件，再把路径交给 QML。
 *
 * ── 用法（MusicController 侧）────────────────────────────────────────────
 *     pathFor(url)：已缓存返回 file:// 路径；否则入队下载并返回空串。
 *     下载完成后发 ready()，controller 转成模型字段刷新
 *     （QML 的 ListItemComponent 里引用不到页面属性，所以走模型字段，
 *       这是 BBTieba 验证过的做法）。
 *
 * 缓存目录用 QDir::tempPath() 下的子目录，跨启动复用。
 */

#ifndef NM_IMAGE_CACHE_HPP
#define NM_IMAGE_CACHE_HPP

#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>

#include <QNetworkReply>
#include <QSslError>

class QNetworkAccessManager;

namespace nm {

class NmImageCache : public QObject
{
    Q_OBJECT
public:
    explicit NmImageCache(QObject *parent = 0);
    virtual ~NmImageCache();

    /*!
     * 取某个 URL 的本地缓存路径（file:// 形式）。
     * 已缓存则直接返回；否则触发一次下载并返回空串。
     */
    QString pathFor(const QString &url);

    /*!
     * 只取消【排队中】的下载（已完成的缓存、正在下的那几个都不动）。
     *
     * ★ 为什么需要：队列有上限（kMaxQueue），如果上一个列表（几百首歌
     *   的封面）把队列占满，新列表（比如 MV 封面）就排不进去，
     *   表现是"新列表封面全是占位图"。切列表时先调这个，新列表的图
     *   就能优先下载。
     */
    void cancelQueued();

    /*!
     * 清空缓存：删掉磁盘上的缓存文件并重置内存表。
     * （设置页的「清除图片缓存」用它）
     * @return 删掉的文件数
     */
    int clear();

    /*! 缓存目录里的字节数（设置页显示用） */
    qint64 cacheSizeBytes() const;

    /*! 覆盖缓存目录（测试用）。为空时用系统临时目录。 */
    void setCacheDir(const QString &dir) { m_cacheDir = dir; }
    QString cacheDir() const { return m_effectiveDir.isEmpty() ? m_cacheDir
                                                              : m_effectiveDir; }

    /*! 已缓存的文件数（排查用） */
    int cachedCount() const { return m_done.size(); }
    /*! 正在下载的数量 */
    int activeCount() const { return m_active.size(); }
    /*! 排队等待下载的数量 */
    int queuedCount() const { return m_queue.size(); }

    /*!
     * 同时下载的最大数量。
     * 一个歌单几百首歌，不限流会同时发出几十个请求把 UI 拖住。
     */
    void setMaxConcurrent(int n);
    int maxConcurrent() const { return m_maxConcurrent; }

    /*! 下载超时（毫秒） */
    void setTimeout(int ms) { m_timeoutMs = ms; }

    /*!
     * 落盘前把图缩到这个边长以内（像素，0 = 不缩放）。
     *
     * ★ 这是列表滑动流畅度的关键（性能实测思路）：
     *   网易的封面原图动辄 500KB、上千像素，而列表里只显示 80~100
     *   du。每次滚动到可视区都要解码一遍大图，UI 线程直接被拖住。
     *   存成 160px 的小图后，解码开销降一个数量级。
     */
    void setMaxImageSize(int px) { m_maxImageSize = px; }
    int maxImageSize() const { return m_maxImageSize; }

    /*! 详细日志开关（排查封面问题时打开） */
    void setVerbose(bool on) { m_verbose = on; }
    bool isVerbose() const { return m_verbose; }

    /*! 自检：内部信号连接是否有效（签名是否解析得到） */
    bool isWired() const { return m_wired; }

signals:
    /*! 有新图片下载完成，QML 应重新求值绑定 */
    void ready();

private slots:
    /*!
     * 下载完成。
     * ★ 必须带 QNetworkReply* 参数：manager 的 finished(QNetworkReply*)
     *   由参数给出 reply；用 sender() 拿到的是 manager，qobject_cast
     *   会静默失败（BBTieba 踩过，别改回去）。
     */
    void onFinished(QNetworkReply *reply);
    /*! 下载超时：把 URL 从"下载中"摘掉，下次请求会重试 */
    void onTimeout();
    /*! 网络层错误（DNS/TLS/连接被拒等） */
    void onReplyError(QNetworkReply::NetworkError code);
    /*! SSL 握手错误：打日志 + 忽略（BB10 根证书库较旧，封面是公开资源） */
    void onReplySslErrors(const QList<QSslError> &errors);

private:
    QString ensureCacheDir() const;
    static QString fileNameFor(const QString &url, int maxImageSize);
    void startDownload(const QString &url);
    void pumpQueue();
    /*!
     * 下载失败的兜底重试。
     *
     * ★ 为什么需要（真机实测）：带 ?imageView=...&thumbnail=... 这类
     *   加工参数的封面，在设备上会被 CDN 拒掉（status 400），
     *   而同一个 URL 去掉问号后面那段就能正常下（PC 实测 200）。
     *   所以失败时先试试"裸图地址"，只重试一次。
     */
    bool retryStripped(const QString &url);

    QNetworkAccessManager *m_manager;
    QString m_cacheDir;
    int m_timeoutMs;
    int m_maxConcurrent;
    /*! 落盘前的最大边长（见 setMaxImageSize 的说明） */
    int m_maxImageSize;
    /*! url -> 本地路径（已完成），同时充当内存缓存 */
    QHash<QString, QString> m_done;
    /*! 正在下载的 url -> reply */
    QHash<QString, QNetworkReply *> m_active;
    /*! 排队等待下载的 url */
    QStringList m_queue;
    /*! 查过但还没拿到本地路径的 url（防重复入队） */
    QSet<QString> m_requested;
    /*! 已经用"去掉 ? 参数"的方式重试过的地址（只重试一次） */
    QSet<QString> m_retried;
    /*! 下载失败的地址：本次运行内不再重试（免得同一条 404 反复刷） */
    QSet<QString> m_failed;
    bool m_verbose;
    mutable QString m_effectiveDir;
    bool m_wired;
};

} // namespace nm

#endif // NM_IMAGE_CACHE_HPP
