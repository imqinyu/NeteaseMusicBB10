/*
 * NmSession - 登录态（MUSIC_U cookie）的存取
 *
 * 网易网页版的登录凭证是 MUSIC_U cookie（浏览器登录 music.163.com 后
 * 在开发者工具里可以看到）。它和贴吧的 BDUSS 一样是「粘贴即登录」的
 * 全权凭证，所以：
 *   - 只存进应用沙箱内的 QSettings，不外发；
 *   - 调试日志里已由 NmHttpClient 做打码。
 *
 * 支持的输入形式（用户手滑粘贴哪种都行）：
 *   - 纯 MUSIC_U 值
 *   - "MUSIC_U=xxx" 单条
 *   - "MUSIC_U=xxx; __csrf=yyy; ..." 整段浏览器 cookie
 *   - "foo=1&MUSIC_U=xxx" 请求参数形式
 */

#ifndef NM_SESSION_HPP
#define NM_SESSION_HPP

#include <QObject>
#include <QString>

class QSettings;

namespace nm {

class NmSession : public QObject
{
    Q_OBJECT
public:
    explicit NmSession(QObject *parent = 0);
    virtual ~NmSession();

    /*! 是否已保存登录凭证 */
    bool hasCookie() const { return !m_musicU.isEmpty(); }

    /*! MUSIC_U 的值 */
    QString musicU() const { return m_musicU; }

    /*!
     * 写入登录凭证。输入形式见文件头。
     * @return 解析出了 MUSIC_U 返回 true
     */
    bool setCookieInput(const QString &input);

    /*! 清除登录态 */
    void clear();

    /*! 从 QSettings 恢复（应用启动时调用一次） */
    void load();

    /*! 持久化（修改后自动调用，也可手动） */
    void save() const;

    /*!
     * 发请求用的完整 Cookie 头。
     * MUSIC_U 之外补上 os/appver 等客户端标识 —— 老接口对它们不敏感，
     * 但带上能减少个别接口的风控概率。
     */
    QString cookieHeader() const;

    /*! 音质偏好（默认 128000，普通账号 320000 通常也会被降档） */
    int bitrate() const { return m_bitrate; }
    void setBitrate(int br);

    /* ===================== 多账号（账号管理页） ===================== */

    /*!
     * 已保存的账号列表（账号管理页用）。
     * 每项字段：id / nickName / userId / avatarUrl / masked / isCurrent。
     * ★ 列表里【存着完整的 MUSIC_U】，但给 QML 的只有打码后的 masked，
     *   真要取明文走 credential()。
     */
    QVariantList accounts() const;

    /*! 当前正在使用的账号（未登录时返回空 map） */
    QVariantMap currentAccount() const;

    /*! 除当前账号以外的已保存账号 */
    QVariantList otherAccounts() const;

    /*!
     * 把当前 MUSIC_U 记成/更新为一个账号。
     * ★ /nuser/account/get 校验成功后才调 —— 那时才知道 userId / 昵称 / 头像。
     */
    void rememberCurrentAccount(qint64 userId, const QString &nickName,
                                const QString &avatarUrl);

    /*! 切到某个已保存的账号（只换凭证，校验交给调用方发 fetchAccount） */
    bool switchTo(const QString &id);

    /*! 删除账号；删掉的正好是当前账号时，登录态一并清掉 */
    bool removeAccount(const QString &id);

    /*! 某个账号的 MUSIC_U 明文（账号管理页的「复制凭据」用） */
    QString credential(const QString &id) const;

signals:
    void sessionChanged();
    void bitrateChanged(int br);
    /*! 账号列表 / 当前账号变了（账号管理页据此刷新） */
    void accountsChanged();

private:
    static QString extractMusicU(const QString &input);
    /*! 打码后的凭证（只显示头尾，日志/界面里都别露全文） */
    static QString maskCredential(const QString &musicU);
    /*! 按 id 找账号下标；id 为空时退回按当前 MUSIC_U 匹配（老版本升级那条） */
    int indexOfId(const QString &id) const;

    QSettings *m_settings;
    QString m_musicU;
    int m_bitrate;

    /*! 已保存账号。每项：{id, nickName, userId, avatarUrl, musicU} */
    QVariantList m_accounts;
    /*! 当前账号的 id（空 = 匿名/还没校验出来） */
    QString m_currentId;
};

} // namespace nm

#endif // NM_SESSION_HPP
