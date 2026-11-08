#include "media/MediaFactory.h"

#include "media/CaptureDevices.h"
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

std::shared_ptr<MediaSource> createLiveSource(const std::string& device, const std::string& size, double fps)
{
    VideoOptions options;
    options.live = true;
    options.loop = false;
    options.liveSize = size;
    options.liveFps = fps;
    std::string path = device;
    const std::string testPrefix = "lavfi:";
    if (device.compare(0, testPrefix.size(), testPrefix) == 0) {
        options.inputFormat = "lavfi";
        path = device.substr(testPrefix.size());
    } else {
        options.inputFormat = captureInputFormat();
    }
    return std::make_shared<VideoSource>(path, options);
}

} // namespace led
