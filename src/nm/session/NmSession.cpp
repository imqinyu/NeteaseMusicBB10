#include "session/NmSession.hpp"
#include "util/NmString.hpp"

#include <QLatin1Char>
#include <QLatin1String>
#include <QRegExp>
#include <QSettings>
#include <QStringList>

namespace nm {

namespace {

const char kKeyMusicU[] = "session/musicU";
const char kKeyBitrate[] = "session/bitrate";
const char kKeyAccounts[] = "session/accounts";
const char kKeyCurrentId[] = "session/currentId";
const int kDefaultBitrate = 128000;

} // anonymous namespace

NmSession::NmSession(QObject *parent)
    : QObject(parent)
    , m_settings(new QSettings(this))
    , m_bitrate(kDefaultBitrate)
{
}

NmSession::~NmSession()
{
}

QString NmSession::extractMusicU(const QString &input)
{
    QString text = input.trimmed();
    if (text.isEmpty())
        return QString();

    // 形式 1：纯值（一长串不含 '=' 和 ';' 的字符）
    if (!text.contains(QLatin1Char('=')))
        return text;

    // 形式 2/3：cookie 串或请求参数串，逐段找 MUSIC_U
    // cookie 分隔符是 "; "，请求参数分隔符是 "&"，一起切
    QStringList parts = text.split(QRegExp(QLatin1String("[;&]")),
                                   QString::SkipEmptyParts);
    foreach (const QString &part, parts) {
        const int eq = part.indexOf(QLatin1Char('='));
        if (eq <= 0)
            continue;
        const QString name = part.left(eq).trimmed();
        if (name.compare(QLatin1String("MUSIC_U"), Qt::CaseInsensitive) == 0) {
            QString value = part.mid(eq + 1).trimmed();
            // 去掉可能被一起粘进来的引号
            if (value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"'))
                && value.size() >= 2)
                value = value.mid(1, value.size() - 2);
            return value;
        }
    }

    return QString();
}

bool NmSession::setCookieInput(const QString &input)
{
    const QString value = extractMusicU(input);
    if (value.isEmpty()) {
        emit sessionChanged();
        return false;
    }

    if (value == m_musicU)
        return true;

    m_musicU = value;
    // 新粘贴的凭证还没校验过，先不认它属于哪个账号；
    // /nuser/account/get 回来后会由 rememberCurrentAccount() 认领（或新建）
    m_currentId.clear();
    save();
    emit sessionChanged();
    emit accountsChanged();
    return true;
}

void NmSession::clear()
{
    if (m_musicU.isEmpty() && m_currentId.isEmpty())
        return;

    /*
     * 只清【当前登录态】，不动 m_accounts ——
     * 退出后账号还留在「已登录」列表里，随时能再登回来。
     */
    m_musicU.clear();
    m_currentId.clear();
    save();
    emit sessionChanged();
    emit accountsChanged();
}

void NmSession::load()
{
    m_musicU = m_settings->value(QLatin1String(kKeyMusicU)).toString();
    m_bitrate = m_settings->value(QLatin1String(kKeyBitrate), kDefaultBitrate).toInt();
    if (m_bitrate <= 0)
        m_bitrate = kDefaultBitrate;

    m_accounts = m_settings->value(QLatin1String(kKeyAccounts)).toList();
    m_currentId = m_settings->value(QLatin1String(kKeyCurrentId)).toString();

    /*
     * ★ 老版本升上来的兼容：以前只存了一个裸 MUSIC_U，没有账号列表。
     *   这里补一条占位记录（id 还空着）—— 等 /nuser/account/get 回来，
     *   rememberCurrentAccount() 会按 musicU 找到它，再把 id / 昵称 / 头像补齐。
     *   这样升级不会把已经登录的人挤下线。
     */
    if (m_accounts.isEmpty() && !m_musicU.isEmpty()) {
        QVariantMap acc;
        acc.insert(QLatin1String("id"), QString());
        acc.insert(QLatin1String("musicU"), m_musicU);
        m_accounts.append(acc);
    }
}

void NmSession::save() const
{
    m_settings->setValue(QLatin1String(kKeyMusicU), m_musicU);
    m_settings->setValue(QLatin1String(kKeyBitrate), m_bitrate);
    m_settings->setValue(QLatin1String(kKeyAccounts), m_accounts);
    m_settings->setValue(QLatin1String(kKeyCurrentId), m_currentId);
    m_settings->sync();
}

QString NmSession::cookieHeader() const
{
    if (m_musicU.isEmpty())
        return QString(QLatin1String("os=pc"));

    return QString(QLatin1String("MUSIC_U=%1; os=pc; appver=8.10.05; "
                                 "osver=Microsoft-Windows-10; channel=netease"))
        .arg(m_musicU);
}

void NmSession::setBitrate(int br)
{
    if (br == m_bitrate)
        return;

    m_bitrate = br;
    save();
    emit bitrateChanged(br);
}

// ===================== 多账号（账号管理页） =====================

QString NmSession::maskCredential(const QString &musicU)
{
    if (musicU.isEmpty())
        return QString();
    if (musicU.size() <= 12)
        return QString(QLatin1String("…"));

    // 只露头尾：界面上显示"00AC6E94…4D5A"这种，够用户认出是哪一个就行
    return musicU.left(8) + QLatin1String("…") + musicU.right(4);
}

int NmSession::indexOfId(const QString &id) const
{
    if (! id.isEmpty()) {
        for (int i = 0; i < m_accounts.size(); ++i) {
            if (m_accounts.at(i).toMap().value(QLatin1String("id")).toString() == id)
                return i;
        }
        return -1;
    }

    /*
     * id 为空 = 老版本升级留下的那条占位记录（还没校验出 userId）。
     * 按"当前 MUSIC_U"认 —— 那一瞬间只可能是它。
     */
    if (! m_musicU.isEmpty()) {
        for (int i = 0; i < m_accounts.size(); ++i) {
            if (m_accounts.at(i).toMap().value(QLatin1String("musicU")).toString()
                    == m_musicU)
                return i;
        }
    }
    return -1;
}

QVariantList NmSession::accounts() const
{
    QVariantList out;
    foreach (const QVariant &item, m_accounts) {
        const QVariantMap acc = item.toMap();
        QVariantMap row = acc;
        // ★ 明文凭证不经 QML 出去（要明文走 credential()）
        row.remove(QLatin1String("musicU"));
        row.insert(QLatin1String("masked"),
                   maskCredential(acc.value(QLatin1String("musicU")).toString()));
        row.insert(QLatin1String("isCurrent"),
                   ! m_musicU.isEmpty()
                   && acc.value(QLatin1String("musicU")).toString() == m_musicU);
        out.append(row);
    }
    return out;
}

QVariantMap NmSession::currentAccount() const
{
    if (m_musicU.isEmpty())
        return QVariantMap();

    const int idx = indexOfId(m_currentId);
    if (idx < 0)
        return QVariantMap();

    QVariantMap row = m_accounts.at(idx).toMap();
    row.remove(QLatin1String("musicU"));
    row.insert(QLatin1String("masked"), maskCredential(m_musicU));
    row.insert(QLatin1String("isCurrent"), true);
    return row;
}

QVariantList NmSession::otherAccounts() const
{
    QVariantList out;
    foreach (const QVariant &item, accounts()) {
        const QVariantMap row = item.toMap();
        if (! row.value(QLatin1String("isCurrent")).toBool())
            out.append(row);
    }
    return out;
}

void NmSession::rememberCurrentAccount(qint64 userId, const QString &nickName,
                                       const QString &avatarUrl)
{
    if (m_musicU.isEmpty())
        return;

    const QString id = (userId > 0) ? QString::number(userId) : QString();

    // 先按 id 找；找不到再按 musicU 找（升级兼容 / 重贴同一份凭证）
    int idx = id.isEmpty() ? -1 : indexOfId(id);
    if (idx < 0) {
        for (int i = 0; i < m_accounts.size(); ++i) {
            if (m_accounts.at(i).toMap().value(QLatin1String("musicU")).toString()
                    == m_musicU) {
                idx = i;
                break;
            }
        }
    }

    QVariantMap acc = (idx >= 0) ? m_accounts.at(idx).toMap() : QVariantMap();
    if (! id.isEmpty())
        acc.insert(QLatin1String("id"), id);
    if (! nickName.isEmpty())
        acc.insert(QLatin1String("nickName"), nickName);
    const QString cleanAvatar = NmString::cleanAvatarUrl(avatarUrl);
    if (! cleanAvatar.isEmpty())
        acc.insert(QLatin1String("avatarUrl"), cleanAvatar);
    if (userId > 0)
        acc.insert(QLatin1String("userId"), QString::number(userId));
    acc.insert(QLatin1String("musicU"), m_musicU);

    if (idx >= 0)
        m_accounts.replace(idx, acc);
    else
        m_accounts.append(acc);

    m_currentId = acc.value(QLatin1String("id")).toString();

    save();
    emit accountsChanged();
}

bool NmSession::switchTo(const QString &id)
{
    const int idx = indexOfId(id);
    if (idx < 0)
        return false;

    const QVariantMap acc = m_accounts.at(idx).toMap();
    const QString musicU = acc.value(QLatin1String("musicU")).toString();
    if (musicU.isEmpty())
        return false;

    m_musicU = musicU;
    m_currentId = acc.value(QLatin1String("id")).toString();
    save();
    emit sessionChanged();
    emit accountsChanged();
    return true;
}

bool NmSession::removeAccount(const QString &id)
{
    const int idx = indexOfId(id);
    if (idx < 0)
        return false;

    const bool wasCurrent = ! m_musicU.isEmpty()
        && m_accounts.at(idx).toMap().value(QLatin1String("musicU")).toString() == m_musicU;

    m_accounts.removeAt(idx);

    if (wasCurrent) {
        // 删掉的正是当前在用的：登录态一起清掉，
        // 否则手里还攥着一份已经删了的凭证继续发请求
        m_musicU.clear();
        m_currentId.clear();
        emit sessionChanged();
    }
    save();
    emit accountsChanged();
    return true;
}

QString NmSession::credential(const QString &id) const
{
    const int idx = indexOfId(id);
    if (idx < 0)
        return QString();
    return m_accounts.at(idx).toMap().value(QLatin1String("musicU")).toString();
}

} // namespace nm
