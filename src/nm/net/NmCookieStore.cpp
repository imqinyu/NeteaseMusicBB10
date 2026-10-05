#include "NmCookieStore.hpp"

#include <QStringList>
#include <QUrl>

namespace nm {

namespace {

/*! 从 URL 取主机名（小写，不含端口） */
QString hostOf(const QString &url)
{
    const QUrl parsed(url);
    QString host = parsed.host().toLower();
    if (host.isEmpty()) {
        // QUrl 对畸形输入可能解析失败，退化为手工截取
        const int scheme = url.indexOf(QLatin1String("://"));
        const int start = scheme >= 0 ? scheme + 3 : 0;
        const int end = url.indexOf(QLatin1Char('/'), start);
        host = (end < 0 ? url.mid(start) : url.mid(start, end - start)).toLower();
        const int colon = host.indexOf(QLatin1Char(':'));
        if (colon >= 0)
            host = host.left(colon);
    }
    return host;
}

/*! 从 URL 取路径（默认 "/"） */
QString pathOf(const QString &url)
{
    const QUrl parsed(url);
    QString path = parsed.path();
    if (path.isEmpty())
        path = QLatin1String("/");
    return path;
}

/*! 解析 HTTP 日期（RFC 1123 与常见变体） */
QDateTime parseHttpDate(const QString &text)
{
    const QString value = text.trimmed();

    // 先试标准格式
    QDateTime dt = QDateTime::fromString(value, QLatin1String("ddd, dd MMM yyyy HH:mm:ss GMT"));
    if (dt.isValid())
        return dt;

    // 有些服务器发 "dd-MMM-yyyy HH:mm:ss" 或带 "-" 的旧格式
    static const char *formats[] = {
        "ddd, dd-MMM-yyyy HH:mm:ss GMT",
        "ddd, dd MMM yyyy HH:mm:ss 'GMT'",
        "dd MMM yyyy HH:mm:ss",
        "ddd MMM d HH:mm:ss yyyy"
    };
    for (unsigned i = 0; i < sizeof(formats) / sizeof(formats[0]); ++i) {
        dt = QDateTime::fromString(value, QLatin1String(formats[i]));
        if (dt.isValid())
            return dt;
    }
    return QDateTime();
}

} // anonymous namespace

NmCookieStore::NmCookieStore()
{
}

void NmCookieStore::setFromHeader(const QString &setCookieValue, const QString &defaultHost)
{
    // Set-Cookie: name=value; Path=/; Domain=.nm.baidu.com; Expires=...; Secure; HttpOnly
    const QStringList segments = setCookieValue.split(QLatin1Char(';'));
    if (segments.isEmpty())
        return;

    const QString first = segments.at(0).trimmed();
    const int eq = first.indexOf(QLatin1Char('='));
    if (eq <= 0)
        return;

    NmCookie cookie;
    cookie.name = first.left(eq).trimmed();
    cookie.value = first.mid(eq + 1).trimmed();
    cookie.domain = defaultHost.toLower();
    cookie.path = QLatin1String("/");
    cookie.hostOnly = true;

    if (cookie.name.isEmpty())
        return;

    for (int i = 1; i < segments.size(); ++i) {
        const QString segment = segments.at(i).trimmed();
        const int sep = segment.indexOf(QLatin1Char('='));
        const QString key = (sep < 0 ? segment : segment.left(sep)).trimmed().toLower();
        const QString value = (sep < 0 ? QString() : segment.mid(sep + 1)).trimmed();

        if (key == QLatin1String("domain") && !value.isEmpty()) {
            QString domain = value.toLower();
            if (domain.startsWith(QLatin1Char('.')))
                domain = domain.mid(1);
            cookie.domain = domain;
            cookie.hostOnly = false;
        } else if (key == QLatin1String("path") && !value.isEmpty()) {
            cookie.path = value;
        } else if (key == QLatin1String("expires")) {
            const QDateTime dt = parseHttpDate(value);
            if (dt.isValid())
                cookie.expires = dt;
        } else if (key == QLatin1String("max-age")) {
            bool ok = false;
            const int seconds = value.toInt(&ok);
            if (ok) {
                if (seconds <= 0)
                    cookie.expires = QDateTime::currentDateTime().addSecs(-1);
                else
                    cookie.expires = QDateTime::currentDateTime().addSecs(seconds);
            }
        } else if (key == QLatin1String("secure")) {
            cookie.secure = true;
        }
    }

    store(cookie);
}

void NmCookieStore::store(const NmCookie &cookie)
{
    // 已过期等价于删除
    if (cookie.expires.isValid()
            && cookie.expires < QDateTime::currentDateTime()) {
        for (int i = m_cookies.size() - 1; i >= 0; --i) {
            if (m_cookies.at(i).name == cookie.name
                    && m_cookies.at(i).domain == cookie.domain
                    && m_cookies.at(i).path == cookie.path) {
                m_cookies.removeAt(i);
            }
        }
        return;
    }

    for (int i = 0; i < m_cookies.size(); ++i) {
        NmCookie &existing = m_cookies[i];
        if (existing.name == cookie.name
                && existing.domain == cookie.domain
                && existing.path == cookie.path) {
            existing = cookie;
            return;
        }
    }
    m_cookies.append(cookie);
}

bool NmCookieStore::domainMatches(const QString &host, const NmCookie &cookie)
{
    if (cookie.domain.isEmpty())
        return false;
    if (cookie.hostOnly)
        return host == cookie.domain;
    if (host == cookie.domain)
        return true;
    // 子域匹配：必须以 ".domain" 结尾
    return host.endsWith(QLatin1Char('.') + cookie.domain);
}

QString NmCookieStore::cookieHeaderFor(const QString &url) const
{
    const QString host = hostOf(url);
    const QString path = pathOf(url);

    QStringList parts;
    for (int i = 0; i < m_cookies.size(); ++i) {
        const NmCookie &cookie = m_cookies.at(i);
        if (!domainMatches(host, cookie))
            continue;
        if (!path.startsWith(cookie.path))
            continue;
        parts.append(cookie.name + QLatin1Char('=') + cookie.value);
    }
    return parts.join(QLatin1String("; "));
}

QString NmCookieStore::value(const QString &name) const
{
    for (int i = 0; i < m_cookies.size(); ++i) {
        if (m_cookies.at(i).name == name)
            return m_cookies.at(i).value;
    }
    return QString();
}

void NmCookieStore::clear()
{
    m_cookies.clear();
}

void NmCookieStore::purgeExpired()
{
    const QDateTime now = QDateTime::currentDateTime();
    for (int i = m_cookies.size() - 1; i >= 0; --i) {
        const NmCookie &cookie = m_cookies.at(i);
        if (cookie.expires.isValid() && cookie.expires < now)
            m_cookies.removeAt(i);
    }
}

QString NmCookieStore::serialize() const
{
    // 格式：name=value; name2=value2
    // cookie 值里可能有 ';' 或 '='，这里只保存登录相关的简单 cookie，
    // 因此够用；读回时按第一个 '=' 切分。
    QStringList parts;
    for (int i = 0; i < m_cookies.size(); ++i) {
        const NmCookie &cookie = m_cookies.at(i);
        parts.append(cookie.name + QLatin1Char('=') + cookie.value);
    }
    return parts.join(QLatin1String("; "));
}

void NmCookieStore::restore(const QString &data)
{
    m_cookies.clear();
    if (data.isEmpty())
        return;

    const QStringList parts = data.split(QLatin1Char(';'), QString::SkipEmptyParts);
    for (int i = 0; i < parts.size(); ++i) {
        const QString part = parts.at(i).trimmed();
        const int eq = part.indexOf(QLatin1Char('='));
        if (eq <= 0)
            continue;
        NmCookie cookie;
        cookie.name = part.left(eq).trimmed();
        cookie.value = part.mid(eq + 1).trimmed();
        // 恢复的 cookie 不区分域，统一挂到贴吧主域
        cookie.domain = QLatin1String("nm.baidu.com");
        cookie.hostOnly = false;
        cookie.path = QLatin1String("/");
        m_cookies.append(cookie);
    }
}

} // namespace nm
