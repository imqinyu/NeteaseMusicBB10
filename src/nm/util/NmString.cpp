#include "NmString.hpp"

#include "util/NmTr.hpp"

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QStringList>
#include <QtGlobal>

#include <stdlib.h>

namespace nm {

namespace {

const char kHexDigits[] = "0123456789ABCDEF";

/*! 按分隔符切出 k=v 对；不做解码 */
QList<KeyValue> splitPairs(const QString &text, QChar separator)
{
    QList<KeyValue> out;
    const QStringList chunks = text.split(separator, QString::SkipEmptyParts);
    for (int i = 0; i < chunks.size(); ++i) {
        const QString &chunk = chunks.at(i);
        const int eq = chunk.indexOf(QLatin1Char('='));
        if (eq < 0)
            out.append(qMakePair(QString(chunk), QString()));
        else
            out.append(qMakePair(QString(chunk.left(eq)), QString(chunk.mid(eq + 1))));
    }
    return out;
}

/*! 字节序比较，保证跨平台（尤其 ARM/桌面）排序结果一致 */
bool byteLessThan(const QString &a, const QString &b)
{
    const QByteArray ba = a.toUtf8();
    const QByteArray bb = b.toUtf8();
    const int n = qMin(ba.size(), bb.size());
    for (int i = 0; i < n; ++i) {
        const unsigned char ca = (unsigned char)ba.at(i);
        const unsigned char cb = (unsigned char)bb.at(i);
        if (ca != cb)
            return ca < cb;
    }
    return ba.size() < bb.size();
}

int hexValue(QChar c)
{
    const ushort u = c.unicode();
    if (u >= '0' && u <= '9') return u - '0';
    if (u >= 'a' && u <= 'f') return u - 'a' + 10;
    if (u >= 'A' && u <= 'F') return u - 'A' + 10;
    return -1;
}

/*!
 * 标签名首字符的合法集合。
 * 用于 stripHtml：正文里真的会出现裸露的 '<'，不能一律当标签起点。
 */
bool isTagStartChar(QChar c)
{
    if (c.isLetter())
        return true;
    return c == QLatin1Char('/') || c == QLatin1Char('!')
        || c == QLatin1Char('?');
}

/*!
 * 从 lt（'<' 的位置）开始尝试匹配一个 HTML 标签，返回 '>' 的下标；
 * 不是标签则返回 -1。
 */
int findTagEnd(const QString &text, int lt)
{
    const int len = text.size();
    int i = lt + 1;
    if (i >= len)
        return -1;

    if (!isTagStartChar(text.at(i)))
        return -1;
    ++i;

    // 扫描到 '>'，中间不允许再出现 '<'（否则说明这不是一个完整标签）
    while (i < len) {
        const QChar c = text.at(i);
        if (c == QLatin1Char('<'))
            return -1;
        if (c == QLatin1Char('>'))
            return i;
        ++i;
    }
    return -1;
}

} // anonymous namespace

bool NmString::isAllowedChar(ushort c)
{
    // 翻译自 OkHttp canonicalize 的否定字符类
    if (c >= 'a' && c <= 'z') return true;
    if (c >= 'A' && c <= 'Z') return true;
    if (c >= '0' && c <= '9') return true;
    switch (c) {
    case '-': case '_': case '.': case '~':
    case '!': case '\'': case '(': case ')': case '*':
        return true;
    default:
        return false;
    }
}

QString NmString::percentEncode(const QString &input, bool keepSlash)
{
    // 先转 UTF-8 字节，再逐字节编码：中文会变成 3 个 %XX
    const QByteArray bytes = input.toUtf8();
    QByteArray out;
    out.reserve(bytes.size() * 3);

    for (int i = 0; i < bytes.size(); ++i) {
        const unsigned char b = (unsigned char)bytes.at(i);
        if (b == ' ') {
            out.append('+');
        } else if (keepSlash && b == '/') {
            out.append('/');
        } else if (b < 0x80 && isAllowedChar((ushort)b)) {
            out.append((char)b);
        } else {
            out.append('%');
            out.append(kHexDigits[(b >> 4) & 0x0f]);
            out.append(kHexDigits[b & 0x0f]);
        }
    }
    return QString::fromLatin1(out.constData(), out.size());
}

QString NmString::percentDecode(const QString &input)
{
    QByteArray out;
    out.reserve(input.size());

    for (int i = 0; i < input.size(); ++i) {
        const QChar c = input.at(i);
        if (c == QLatin1Char('+')) {
            out.append(' ');
            continue;
        }
        if (c == QLatin1Char('%') && i + 2 < input.size()) {
            const int hi = hexValue(input.at(i + 1));
            const int lo = hexValue(input.at(i + 2));
            if (hi >= 0 && lo >= 0) {
                out.append((char)((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        // 非 ASCII 字符本来就已经是解码后的文本，按 UTF-8 追加
        out.append(QString(c).toUtf8());
    }
    return QString::fromUtf8(out.constData(), out.size());
}

QByteArray NmString::buildFormBody(const QList<KeyValue> &params)
{
    QStringList parts;
    for (int i = 0; i < params.size(); ++i) {
        parts.append(params.at(i).first + QLatin1Char('=')
                     + percentEncode(params.at(i).second));
    }
    return parts.join(QLatin1String("&")).toUtf8();
}

QString NmString::buildQuery(const QList<KeyValue> &params)
{
    return QString::fromUtf8(buildFormBody(params).constData());
}

QList<KeyValue> NmString::parseQuery(const QString &query)
{
    QList<KeyValue> out;
    const QList<KeyValue> pairs = splitPairs(query, QLatin1Char('&'));
    for (int i = 0; i < pairs.size(); ++i) {
        out.append(qMakePair(percentDecode(pairs.at(i).first),
                             percentDecode(pairs.at(i).second)));
    }
    return out;
}

QString NmString::sortedRawString(const QList<KeyValue> &params)
{
    // 排序键是「未编码」的 "k=v"（与官方客户端 OkHttp 拦截器的行为一致）
    QStringList raw;
    for (int i = 0; i < params.size(); ++i)
        raw.append(params.at(i).first + QLatin1Char('=') + params.at(i).second);

    // 稳定插入排序，字节序比较，避免依赖平台 locale
    for (int i = 1; i < raw.size(); ++i) {
        const QString key = raw.at(i);
        int j = i - 1;
        while (j >= 0 && byteLessThan(key, raw.at(j))) {
            raw[j + 1] = raw.at(j);
            --j;
        }
        raw[j + 1] = key;
    }
    return raw.join(QLatin1String("&"));
}

QString NmString::sortedEncodedString(const QList<KeyValue> &params)
{
    // 用「未编码」串做排序键，但输出编码后的串
    QList<KeyValue> copy = params;

    for (int i = 1; i < copy.size(); ++i) {
        const KeyValue key = copy.at(i);
        const QString keyRaw = key.first + QLatin1Char('=') + key.second;
        int j = i - 1;
        while (j >= 0) {
            const QString otherRaw = copy.at(j).first + QLatin1Char('=')
                                     + copy.at(j).second;
            if (!byteLessThan(keyRaw, otherRaw))
                break;
            copy[j + 1] = copy.at(j);
            --j;
        }
        copy[j + 1] = key;
    }

    QStringList parts;
    for (int i = 0; i < copy.size(); ++i) {
        parts.append(percentEncode(copy.at(i).first) + QLatin1Char('=')
                     + percentEncode(copy.at(i).second));
    }
    return parts.join(QLatin1String("&"));
}

QString NmString::queryValue(const QString &url, const QString &key)
{
    const int q = url.indexOf(QLatin1Char('?'));
    if (q < 0)
        return QString();
    const QList<KeyValue> params = NmString::parseQuery(url.mid(q + 1));
    for (int i = 0; i < params.size(); ++i) {
        if (params.at(i).first == key)
            return params.at(i).second;
    }
    return QString();
}

QString NmString::httpsImageUrl(const QString &url)
{
    if (url.isEmpty())
        return QString();

    // 已经是 https / asset / file / data 就原样返回
    if (url.startsWith(QLatin1String("https://"))
            || url.startsWith(QLatin1String("asset://"))
            || url.startsWith(QLatin1String("file://"))
            || url.startsWith(QLatin1String("data:")))
        return url;

    // 只升级 scheme，其余原样保留
    if (url.startsWith(QLatin1String("http://")))
        return QLatin1String("https://") + url.mid(7);

    return url;
}

QString NmString::avatarUrl(const QString &portrait)
{
    if (portrait.isEmpty())
        return QString();
    if (portrait.startsWith(QLatin1String("http://"))
            || portrait.startsWith(QLatin1String("https://")))
        return httpsImageUrl(portrait);
    // 注意：这里直接给 https —— BB10 的 ImageView 不认 http
    return QLatin1String("https://tb.himg.baidu.com/sys/portrait/item/") + portrait;
}

QString NmString::bigAvatarUrl(const QString &portrait)
{
    if (portrait.isEmpty())
        return QString();
    if (portrait.startsWith(QLatin1String("http://"))
            || portrait.startsWith(QLatin1String("https://")))
        return httpsImageUrl(portrait);
    return QLatin1String("https://tb.himg.baidu.com/sys/portraith/item/") + portrait;
}

// ★ 网易云未设头像时回的默认灰人图，图 id 固定。真实头像 id 是随机串，不会命中。
static const char *kDefaultAvatarMarker = "109951165793869051";

QString NmString::cleanAvatarUrl(const QString &url)
{
    if (url.isEmpty())
        return QString();
    // 命中默认图 → 当「没头像」处理，QML 退回到本地 avatar_bright.png
    if (url.contains(QLatin1String(kDefaultAvatarMarker), Qt::CaseInsensitive))
        return QString();
    return url;
}

QString NmString::formatUnixTime(qint64 seconds)
{
    if (seconds <= 0)
        return QString();
    const QDateTime dt = QDateTime::fromTime_t((uint)seconds);
    if (!dt.isValid())
        return QString();

    const QDateTime now = QDateTime::currentDateTime();
    const qint64 diff = dt.secsTo(now);
    if (diff >= 0) {
        if (diff < 60)
            return NmTr("E5889AE5889A");
        if (diff < 3600)
            return NmTr("253120E58886E9929FE5898D").arg(diff / 60);
        if (diff < 86400)
            return NmTr("253120E5B08FE697B6E5898D").arg(diff / 3600);
        if (diff < 86400 * 7)
            return NmTr("253120E5A4A9E5898D").arg(diff / 86400);
    }
    if (dt.date().year() == now.date().year())
        return dt.toString(QLatin1String("MM-dd HH:mm"));
    return dt.toString(QLatin1String("yyyy-MM-dd HH:mm"));
}

QString NmString::shortNumber(qint64 value)
{
    if (value < 10000)
        return QString::number(value);
    if (value < 100000000)
        return QString::number((double)value / 10000.0, 'f', 1) + QLatin1String("W");
    return QString::number((double)value / 100000000.0, 'f', 1) + NmTr("E4BABF");
}

QString NmString::stripHtml(const QString &html)
{
    QString out;
    out.reserve(html.size());

    for (int i = 0; i < html.size(); ++i) {
        const QChar c = html.at(i);

        if (c == QLatin1Char('<')) {
            const int tagEnd = findTagEnd(html, i);
            if (tagEnd > i) {
                i = tagEnd;
                continue;
            }
            // 不是完整标签：当成普通 '<' 文本
            out.append(c);
            continue;
        }

        if (c == QLatin1Char('&')) {
            const int semi = html.indexOf(QLatin1Char(';'), i + 1);
            if (semi > i && semi - i <= 8) {
                const QString entity = html.mid(i + 1, semi - i - 1);
                if (entity == QLatin1String("nbsp")) {
                    out.append(QLatin1Char(' '));
                } else if (entity == QLatin1String("lt")) {
                    out.append(QLatin1Char('<'));
                } else if (entity == QLatin1String("gt")) {
                    out.append(QLatin1Char('>'));
                } else if (entity == QLatin1String("amp")) {
                    out.append(QLatin1Char('&'));
                } else if (entity == QLatin1String("quot")) {
                    out.append(QLatin1Char('"'));
                } else if (entity == QLatin1String("apos")) {
                    out.append(QLatin1Char('\''));
                } else if (entity.startsWith(QLatin1String("#"))) {
                    bool ok = false;
                    uint code = 0;
                    if (entity.size() > 1
                            && (entity.at(1) == QLatin1Char('x')
                                || entity.at(1) == QLatin1Char('X'))) {
                        code = entity.mid(2).toUInt(&ok, 16);
                    } else {
                        code = entity.mid(1).toUInt(&ok, 10);
                    }
                    if (ok && code > 0 && code <= 0xFFFF) {
                        out.append(QChar((ushort)code));
                    } else {
                        out.append(QLatin1Char('&') + entity + QLatin1Char(';'));
                    }
                } else {
                    out.append(QLatin1Char('&') + entity + QLatin1Char(';'));
                }
                i = semi;
                continue;
            }
        }

        out.append(c);
    }
    return out;
}

QString NmString::cleanText(const QString &text)
{
    QString out;
    out.reserve(text.size());
    for (int i = 0; i < text.size(); ++i) {
        const ushort u = text.at(i).unicode();
        // 过滤零宽空格 / 零宽连字 / 零宽不连字 / BOM
        if (u == 0x200B || u == 0x200C || u == 0x200D || u == 0xFEFF)
            continue;
        out.append(text.at(i));
    }
    return out.trimmed();
}

QString NmString::randomHex(int bytes)
{
    if (bytes <= 0)
        bytes = 16;
    QByteArray out;
    out.reserve(bytes * 2);
    for (int i = 0; i < bytes; ++i) {
        const unsigned char b = (unsigned char)(rand() & 0xff);
        out.append(kHexDigits[(b >> 4) & 0x0f]);
        out.append(kHexDigits[b & 0x0f]);
    }
    return QString::fromLatin1(out.constData(), out.size());
}

QByteArray NmString::currentTimestampMs()
{
    return QByteArray::number((qint64)QDateTime::currentMSecsSinceEpoch());
}

} // namespace nm
