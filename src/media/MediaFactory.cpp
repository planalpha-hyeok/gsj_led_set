#include "media/MediaFactory.h"

#include "media/ImageSource.h"
#include "media/VideoSource.h"

#include <QFileInfo>
#include <QImageReader>

namespace led {

bool isStillImageFile(const std::string& path)
{
    const QByteArray suffix = QFileInfo(QString::fromStdString(path)).suffix().toLower().toLatin1();
    if (suffix.isEmpty() || suffix == "gif") // GIF는 애니메이션이 있으므로 영상으로 처리
        return false;
    return QImageReader::supportedImageFormats().contains(suffix);
}

std::shared_ptr<MediaSource> createMediaSource(const std::string& path, bool loop, bool hardware)
{
    if (isStillImageFile(path))
        return std::make_shared<ImageSource>(path);
    return std::make_shared<VideoSource>(path, loop, hardware);
}

} // namespace led
