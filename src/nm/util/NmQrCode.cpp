#include "util/NmQrCode.hpp"

#include <QByteArray>
#include <QDir>
#include <QFileInfo>
/*
 * ★ 必须写完整路径 QtGui/QImage：本项目只把 QtCore / QtNetwork /
 *   QtDeclarative 的头文件目录加进了 -I，没加 QtGui 的，所以裸写
 *   <QImage> 会报 No such file or directory。Qt4 支持这种带模块前缀的写法。
 */
#include <QtGui/QImage>

#include "qrcodegen/qrcodegen.h"

namespace nm {

bool NmQrCode::writePng(const QString &text, const QString &outputPath,
                        int scale, int border)
{
    if (text.isEmpty() || outputPath.isEmpty())
        return false;
    if (scale < 1)
        scale = 1;
    if (border < 0)
        border = 0;

    /*
     * 两个缓冲区都按最大版本开：qrcodegen 不用 malloc，空间全由调用方给。
     * （qrcodegen_BUFFER_LEN_MAX 在 qrcodegen.h 里）
     */
    uint8_t qrcode[qrcodegen_BUFFER_LEN_MAX];
    uint8_t tempBuffer[qrcodegen_BUFFER_LEN_MAX];

    const QByteArray payload = text.toUtf8();
    const bool ok = qrcodegen_encodeText(payload.constData(),
                                         tempBuffer, qrcode,
                                         qrcodegen_Ecc_MEDIUM,
                                         qrcodegen_VERSION_MIN,
                                         qrcodegen_VERSION_MAX,
                                         qrcodegen_Mask_AUTO, true);
    if (! ok)
        return false;

    const int size = qrcodegen_getSize(qrcode);
    const int px = (size + border * 2) * scale;

    /*
     * 用 RGB32 而不是 Mono：Mono 的位序（MSB/LSB）在不同平台不一致，
     * 容易出来一张反色或错位的图，不值得为省一点内存去冒这个险。
     */
    QImage image(px, px, QImage::Format_RGB32);
    image.fill(0xFFFFFFFF);                 // 白底（二维码必须浅底深码）

    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            if (! qrcodegen_getModule(qrcode, x, y))
                continue;
            // 一个码点画成 scale × scale 的方块
            for (int dy = 0; dy < scale; ++dy) {
                for (int dx = 0; dx < scale; ++dx) {
                    image.setPixel((x + border) * scale + dx,
                                   (y + border) * scale + dy,
                                   0xFF000000);
                }
            }
        }
    }

    QDir().mkpath(QFileInfo(outputPath).absolutePath());
    return image.save(outputPath, "PNG");
}

} // namespace nm
