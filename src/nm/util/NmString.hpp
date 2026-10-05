/*
 * NmString - URL 编码与贴吧常用字符串处理
 *
 * 贴吧 sign 的计算对参数编码方式敏感，这里必须和官方客户端（OkHttp 的
 * canonicalize 实现）保持一致：
 *
 *   - 允许集合：a-z A-Z 0-9 - _ . ~ 以及 ! ' ( ) *
 *     注意 OkHttp 的 "!\"#$&'(),/:;<=>?@[]\\^`{|}~" 是「否定断言」的字符类，
 *     这里逐字符翻译为上面的肯定集合；
 *   - 空格 -> '+'
 *   - 其他字节 -> %XX，十六进制大写
 *
 * 且编码是按 UTF-8 字节做的，中文会变成 3 个 %XX。
 */

#ifndef NM_STRING_HPP
#define NM_STRING_HPP

#include <QByteArray>
#include <QList>
#include <QPair>
#include <QString>

namespace nm {

typedef QPair<QString, QString> KeyValue;

class NmString
{
public:
    /*! 百分号编码。keepSlash 为 true 时保留 '/'（用于拼 URL 路径） */
    static QString percentEncode(const QString &input, bool keepSlash = false);

    /*! 百分号解码（'+' 视为空格） */
    static QString percentDecode(const QString &input);

    /*! 把 key=value 列表编码成表单体：a=1&b=2 */
    static QByteArray buildFormBody(const QList<KeyValue> &params);

    /*! 把 key=value 列表编码成查询串（与表单体同规则） */
    static QString buildQuery(const QList<KeyValue> &params);

    /*! 解析 a=1&b=2 形式的表单/查询串 */
    static QList<KeyValue> parseQuery(const QString &query);

    /*!
     * 贴吧 sign 的排序规则：按「未编码」的 key=value 做字节序升序。
     * 返回排序后的原始串（"k=v" 之间用 & 连接，未编码）。
     */
    static QString sortedRawString(const QList<KeyValue> &params);

    /*!
     * 贴吧 sign 的编码串：与 sortedRawString 同序，但使用百分号编码后的值。
     * 这是实际发给服务器的参数顺序。
     */
    static QString sortedEncodedString(const QList<KeyValue> &params);

    /*! 从 URL 中取出查询串里的某个参数（百分号解码后） */
    static QString queryValue(const QString &url, const QString &key);

    /*! 头像：portrait -> 完整头像 URL */
    static QString avatarUrl(const QString &portrait);

    /*! 大头像 */
    static QString bigAvatarUrl(const QString &portrait);

    /*! 把「网易云默认头像」清成空串。
     *  没设头像的用户，接口会回一个固定的灰人默认图；清空后各页 QML 会退回到本地
     *  avatar_bright.png（见各页头像 ImageView 的 imageSource fallback）。
     *  ★ kDefaultAvatarMarker 是已知默认图的图 id；若以后网易换地址，
     *    抓一条默认 avatarUrl 替换这一处即可（真实头像 id 是随机串，不会误命中）。 */
    static QString cleanAvatarUrl(const QString &url);

    /*!
     * 把图片 URL 规范化成设备能加载的形式。
     *
     * ★ 为什么必须做：
     *   BB10 的图片加载器**只接受 https / asset / file 方案**，
     *   遇到 http:// 会直接拒绝并打印
     *       "Unsupported scheme (http) used in url (...). Image loading aborted."
     *   而贴吧接口返回的图片地址**可能是 http**（实测：同一接口在开发机上
     *   拿到的是 https，设备上拿到的是 http，服务端按接入点/请求头变化）。
     *
     *   百度的图片 CDN（nmpic / imgsrc / tb.himg 等）都支持 https，
     *   所以这里直接把 scheme 升级即可。
     *
     * 只改 scheme，不动 host/路径/查询串。
     */
    static QString httpsImageUrl(const QString &url);

    /*! 秒 -> "yyyy-MM-dd HH:mm" 本地时间字符串 */
    static QString formatUnixTime(qint64 seconds);

    /*! 数字友好化：12345 -> 1.2W（与官方客户端一致的展示习惯） */
    static QString shortNumber(qint64 value);

    /*! 从 HTML 片段里剥掉标签并还原常见实体（贴吧简介/吧规会带 HTML） */
    static QString stripHtml(const QString &html);

    /*! 去掉首尾空白与零宽字符（贴吧标题里真的有） */
    static QString cleanText(const QString &text);

    /*! 生成 32 位小写十六进制随机串 */
    static QString randomHex(int bytes = 16);

    /*! 当前时间戳（毫秒），贴吧 timestamp 参数用 */
    static QByteArray currentTimestampMs();

private:
    static bool isAllowedChar(ushort c);
};

} // namespace nm

#endif // NM_STRING_HPP
