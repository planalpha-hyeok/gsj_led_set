#include "media/QImageFrame.h"

#include <cstring>

namespace led {

std::shared_ptr<Frame> frameFromQImage(const QImage& image)
{
    if (image.isNull())
        return nullptr;

    // ARGB32_Premultiplied는 리틀엔디언(Windows x86/x64)에서 메모리 순서가 B,G,R,A
    const QImage img = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    if (img.isNull())
        return nullptr;

    auto frame = std::make_shared<Frame>();
    frame->width = img.width();
    frame->height = img.height();
    frame->stride = static_cast<int>(img.bytesPerLine());
    frame->premultiplied = true;
    frame->pixels.resize(static_cast<std::size_t>(frame->stride) * static_cast<std::size_t>(frame->height));
    for (int y = 0; y < frame->height; ++y)
        std::memcpy(frame->pixels.data() + static_cast<std::size_t>(y) * frame->stride, img.constScanLine(y),
                    static_cast<std::size_t>(frame->stride));
    frame->serial = nextFrameSerial();
    return frame;
}

} // namespace led
