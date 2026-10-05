/*
 * NmTr - 用于界面文案的确定性 UTF-8 文本构造
 *
 * 为什么需要这个（走过的弯路，别再改回去）：
 *
 *   BB10 的 QNX gcc 在编译期会按「执行字符集」重新编码字符串字面量，实测结果是
 *   源码里任何一种非 ASCII 写法都可能在运行时变成乱码：
 *
 *     QString::fromUtf8("中文")        -> 源文件字节被重新编码，乱码
 *     QString::fromUtf8("\u4E2D\u6587")-> \u 展开后仍被执行字符集转换，乱码
 *     QString("中文")                  -> 还会再叠一层 Latin-1 隐式转换
 *
 *   而 QML 侧完全正常（QML 引擎自己按 UTF-8 读文件），所以问题是 C++ 独有的。
 *
 * 解法：源码里只出现 ASCII —— 把文本写成 UTF-8 字节的十六进制串，运行时解码。
 * 这样编译期不做任何字符集转换，也不受源文件编码影响：
 *
 *     NmTr("E4B8ADE69687")   ==  QString::fromUtf8("中文")
 *
 * 配套工具：tests\hexify-literals.ps1（把中文字面量自动转成这种形式）。
 * 用 tests\lint-encoding.ps1 校验不要再退回到非 ASCII 字面量。
 *
 * 注意：这里故意不用 QObject::tr()。
 *   一是 tr() 会去查 .qm 翻译表，而 .ts/.qm 是历史遗留、内容已损坏；
 *   二是文案还没做真正的本地化，先用确定能显示的写法。
 *   以后要做多语言时，把这个函数换成 tr() 即可（接口一致）。
 */

#ifndef NM_TR_HPP
#define NM_TR_HPP

#include <QByteArray>
#include <QString>

namespace nm {

/*!
 * 把 UTF-8 字节的十六进制串解码成 QString。
 * @param hex 形如 "E4B8ADE69687"（大小写均可，长度必须为偶数）
 */
inline QString nmFromHex(const char *hex)
{
    if (!hex)
        return QString();

    const QByteArray bytes = QByteArray::fromHex(QByteArray(hex));
    return QString::fromUtf8(bytes.constData(), bytes.size());
}

} // namespace nm

/*!
 * 界面文案的统一写法。
 * 参数是 ASCII 十六进制串，内容是这段文本的 UTF-8 字节。
 */
#define NmTr(hex) ::nm::nmFromHex(hex)

#endif // NM_TR_HPP
