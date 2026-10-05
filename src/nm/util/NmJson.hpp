/*
 * NmJson - 最小但健壮的 JSON 解析器
 *
 * 为什么不用 bb::data::JsonDataAccess：
 *  1. JsonDataAccess 只在 BB10 上存在，导致整个后端层无法在桌面 Qt 上编译/测试；
 *  2. 它会把数字统一变成 int64/UInt64，布尔变成 bool，而贴吧接口里同一字段
 *     在不同接口/不同版本下会在 "1" / 1 / true 之间摇摆，需要统一的容错取值。
 *
 * 设计要点：
 *  - 只解析（本项目不需要序列化）；
 *  - 数字统一保留原始字面量：整数字面量走 qint64（贴吧 ID 常常超过 int32），
 *    带小数点/指数的走 double；需要字符串时直接取原始字面量，做到零精度损失；
 *  - 严格 UTF-8：直接交给 QString::fromUtf8，不自己写解码器（避免中文乱码风险）；
 *  - 错误带行列号，便于真机排查。
 *
 * 用法：
 *     nm::NmJson json = nm::NmJson::parse(bytes);
 *     if (!json.isValid()) { ... json.errorMessage() ... }
 *     const QString name = nm::NmJson::str(json, "forum", "name");
 */

#ifndef NM_JSON_HPP
#define NM_JSON_HPP

#include <QByteArray>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

namespace nm {

class NmJson
{
public:
    enum Type {
        Null,
        Bool,
        Number,
        String,
        Array,
        Object
    };

    NmJson();
    NmJson(const NmJson &other);
    NmJson &operator=(const NmJson &other);
    ~NmJson();

    /*! 解析 UTF-8 JSON 文本。失败时返回 isValid()==false 的对象 */
    static NmJson parse(const QByteArray &utf8);

    bool isValid() const { return m_valid; }
    QString errorMessage() const { return m_error; }

    Type type() const { return m_type; }
    bool isNull() const { return m_type == Null; }
    bool isArray() const { return m_type == Array; }
    bool isObject() const { return m_type == Object; }
    bool isString() const { return m_type == String; }
    bool isNumber() const { return m_type == Number; }
    bool isBool() const { return m_type == Bool; }

    /*! 数组长度；非数组返回 0 */
    int size() const { return m_items.size(); }

    /*! 数组下标访问；越界返回 Null 值 */
    NmJson at(int index) const;

    /*! 对象成员访问；不存在返回 Null 值 */
    NmJson member(const QString &key) const;

    /*! 对象是否包含该键（即使值为 null 也算包含） */
    bool contains(const QString &key) const { return m_members.contains(key); }

    /*! 对象的所有键 */
    QStringList keys() const { return m_members.keys(); }

    /*! 遍历数组元素 */
    QList<NmJson> items() const { return m_items; }

    /*! 遍历对象成员（顺序不保证，QMap 按 key 排序） */
    QMap<QString, NmJson> members() const { return m_members; }

    /* ---- 容错取值：贴吧字段类型不稳定，这里统一处理 ---- */

    /*! 取字符串。数字取原始字面量；bool 取 "true"/"false"；null/缺失返回 def */
    QString toString(const QString &def = QString()) const;

    /*! 取整数。字符串会尝试 toLongLong；bool 取 0/1 */
    qint64 toLongLong(qint64 def = 0) const;

    /*! 取整数，用户等级/楼层这类场景 */
    int toInt(int def = 0) const;

    /*! 取浮点 */
    double toDouble(double def = 0.0) const;

    /*! 取布尔。"1"/"true"/1/true 均为真，"0"/"false"/"" 为假 */
    bool toBool(bool def = false) const;

    /*! 是否为缺失或 null */
    bool isMissing() const { return m_type == Null; }

    /* ---- 链式容错取值：字段不存在或类型不符时返回默认值，不抛异常 ---- */

    /*! 取 str(node, key)，缺失时返回 def */
    static QString str(const NmJson &node, const QString &key,
                       const QString &def = QString());

    /*! 双键回退：key1 缺失时改取 key2（贴吧同一语义字段名在不同接口会变） */
    static QString str2(const NmJson &node, const QString &key1,
                        const QString &key2, const QString &def = QString());
    static qint64 integer(const NmJson &node, const QString &key, qint64 def = 0);
    static int intValue(const NmJson &node, const QString &key, int def = 0);
    static bool boolean(const NmJson &node, const QString &key, bool def = false);
    static NmJson object(const NmJson &node, const QString &key);
    static NmJson array(const NmJson &node, const QString &key);

    /*! 把数组转成字符串列表（忽略非字符串元素） */
    static QStringList stringList(const NmJson &node, const QString &key);

private:
    /*! 递归下降解析器。作为嵌套类以获得对私有成员的访问权 */
    class Parser;

    Type m_type;
    bool m_valid;
    QString m_error;

    bool m_boolValue;
    QString m_stringValue;   // String 原始值 / Number 原始字面量
    bool m_numberIsReal;
    qint64 m_intValue;
    double m_realValue;

    QList<NmJson> m_items;
    QMap<QString, NmJson> m_members;
};

} // namespace nm

#endif // NM_JSON_HPP
