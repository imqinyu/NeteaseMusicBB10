/*
 * NmQrCode - 生成二维码图片（扫码登录用）
 *
 * ★★ 为什么自己生成，而不是让服务端给图
 *   网易确实有个 /login/qr/create?qrimg=true 能直接返回二维码的 base64，
 *   但它属于【新版】接口族；我们这套接口走的是参考项目（MeeGo 版
 *   cloudmusicqt）验证过的【老接口族】/api/login/qrcode/ 下面这几个，
 *   两套不保证混用可用。而本地生成只依赖已验证的 unikey 接口，不赌额外接口。
 *
 * ★★ 为什么用 qrcodegen 而不是 NDK 自带的 zxing
 *   BB10 的 NDK 里确实有 zxing（官方样例 custombarcodescanner 就在用），
 *   但那是【解码】库 —— 头文件里只有一堆 *Reader，没有任何 Writer/Encoder，
 *   生成不了码。所以二维码得自己画。
 *
 *   qrcodegen（Project Nayuki，MIT）是纯 C、不用 malloc、只靠调用方给的
 *   缓冲区，很适合塞进这种老工具链（源码在 util/qrcodegen/）。
 *
 * 用法：writePng("https://music.163.com/login?codekey=xxx", "/tmp/qr.png")
 *   —— 输出一张白底黑码的 PNG，交给 QML 的 ImageView 显示。
 */

#ifndef NM_QRCODE_HPP
#define NM_QRCODE_HPP

#include <QString>

namespace nm {

class NmQrCode
{
public:
    /*!
     * 把 text 编成二维码并写成 PNG 文件。
     *
     * @param text        要编码的内容（网易扫码登录就是那个登录 URL）
     * @param outputPath  输出文件（父目录不存在会自动创建）
     * @param scale       每个"码点"占几个像素（越大越清晰，默认 8）
     * @param border      静区（码点四周留白，QR 规范要求 >= 4，默认 4）
     * @return 成功返回 true
     */
    static bool writePng(const QString &text, const QString &outputPath,
                         int scale = 8, int border = 4);
};

} // namespace nm

#endif // NM_QRCODE_HPP
