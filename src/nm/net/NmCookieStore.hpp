/*
 * NmCookieStore - 极简 cookie 存储
 *
 * 贴吧的登录态主要由 BDUSS 参数承载，cookie 只是次要补充（ka=open 等），
 * 所以这里只做够用为止的实现：
 *   - 解析 Set-Cookie 响应头
 *   - 按 host + path 前缀匹配
 *   - 支持删除（Max-Age=0 / expires 过去）
 *   - 可序列化进 QSettings
 */

#ifndef NM_COOKIE_STORE_HPP
#define NM_COOKIE_STORE_HPP

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

namespace nm {

struct NmCookie
{
    QString name;
    QString value;
    QString domain;   // 不含前导点
    QString path;
    bool hostOnly;
    bool secure;
    QDateTime expires; // 无效表示会话 cookie

    NmCookie()
        : hostOnly(true)
        , secure(false)
    {
    }
};

class NmCookieStore
{
public:
    NmCookieStore();

    /*! 解析一条 Set-Cookie（defaultHost 用于补全 domain） */
    void setFromHeader(const QString &setCookieValue, const QString &defaultHost);

    /*! 针对某 URL 生成 Cookie 请求头；无可用 cookie 时返回空串 */
    QString cookieHeaderFor(const QString &url) const;

    /*! 取某个 cookie 的值 */
    QString value(const QString &name) const;

    /*! 全部清空（登出用） */
    void clear();

    /*! 输出为 "k=v; k2=v2"（用于序列化） */
    QString serialize() const;
    /*! 从 serialize() 的输出恢复 */
    void restore(const QString &data);

    /*! 所有 cookie（调试用） */
    QList<NmCookie> all() const { return m_cookies; }

private:
    void store(const NmCookie &cookie);
    void purgeExpired();
    static bool domainMatches(const QString &host, const NmCookie &cookie);

    QList<NmCookie> m_cookies;
};

} // namespace nm

#endif // NM_COOKIE_STORE_HPP
