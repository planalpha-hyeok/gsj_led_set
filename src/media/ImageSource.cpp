#include "media/ImageSource.h"

#include "core/Log.h"
#include "media/QImageFrame.h"

#include <QImageReader>

namespace led {

ImageSource::ImageSource(std::string path)
    : m_path(std::move(path))
{
}

bool ImageSource::open(std::string& error)
{
    QImageReader reader(QString::fromStdString(m_path));
    reader.setAutoTransform(true); // EXIF 회전 정보 반영
    const QImage image = reader.read();
    if (image.isNull()) {
        error = "이미지를 읽을 수 없음 (" + reader.errorString().toStdString() + ")";
        return false;
    }
    m_frame = frameFromQImage(image);
    if (!m_frame) {
        error = "이미지 변환 실패";
        return false;
    }
    logInfo("이미지 열기: " + m_path + " (" + std::to_string(m_frame->width) + "x" +
            std::to_string(m_frame->height) + ")");
    return true;
}

std::shared_ptr<const Frame> ImageSource::frameAt(double)
{
    return m_frame;
}

bool ImageSource::waitForFirstFrame(int)
{
    return m_frame != nullptr;
}

} // namespace led
