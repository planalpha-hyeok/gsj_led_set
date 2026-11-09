#include "media/MediaInfo.h"

#include "core/LayerContent.h"

#include <algorithm>

#include "media/MediaFactory.h"

extern "C" {
#include <libavformat/avformat.h>
}

#include <QDateTime>
#include <QFileInfo>
#include <QImageReader>

#include <map>
#include <mutex>

namespace led {

MediaInfo probeMedia(const std::string& path)
{
    MediaInfo info;
    if (isStillImageFile(path)) {
        QImageReader reader(QString::fromStdString(path));
        const QSize size = reader.size();
        info.ok = reader.canRead();
        info.still = true;
        info.hasVideo = true;
        info.width = size.width();
        info.height = size.height();
        if (!info.ok)
            info.error = reader.errorString().toStdString();
        return info;
    }

    AVFormatContext* format = nullptr;
    if (avformat_open_input(&format, path.c_str(), nullptr, nullptr) < 0) {
        info.error = "파일을 열 수 없음";
        return info;
    }
    if (avformat_find_stream_info(format, nullptr) >= 0) {
        info.ok = true;
        for (unsigned i = 0; i < format->nb_streams; ++i) {
            const AVStream* s = format->streams[i];
            if (s->disposition & AV_DISPOSITION_ATTACHED_PIC)
                continue; // 음악 파일의 앨범 표지는 영상으로 보지 않음
            if (s->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                info.hasVideo = true;
                info.width = s->codecpar->width;
                info.height = s->codecpar->height;
            } else if (s->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                info.hasAudio = true;
            }
        }
        if (format->duration != AV_NOPTS_VALUE && format->duration > 0)
            info.duration = static_cast<double>(format->duration) / AV_TIME_BASE;
    } else {
        info.error = "스트림 정보를 읽을 수 없음";
    }
    avformat_close_input(&format);
    return info;
}

MediaInfo probeMediaCached(const std::string& path)
{
    static std::mutex mutex;
    static std::map<std::string, std::pair<qint64, MediaInfo>> cache;
    const qint64 stamp = QFileInfo(QString::fromStdString(path)).lastModified().toMSecsSinceEpoch();
    {
        std::lock_guard<std::mutex> lock(mutex);
        const auto it = cache.find(path);
        if (it != cache.end() && it->second.first == stamp)
            return it->second.second;
    }
    MediaInfo info = probeMedia(path);
    std::lock_guard<std::mutex> lock(mutex);
    cache[path] = {stamp, info};
    return info;
}

double resolveClipDuration(const TimelineClip& clip)
{
    switch (clip.kind) {
    case ClipKind::Live: return kDefaultLiveDuration;
    case ClipKind::Ticker: return kDefaultTickerDuration;
    case ClipKind::File: break;
    }
    if (clip.source.empty())
        return 0.0;
    const MediaInfo info = probeMediaCached(clip.source);
    if (!info.ok)
        return 0.0;
    if (info.still || info.duration <= 0.0)
        return kDefaultImageDuration;
    return std::max(0.1, info.duration - clip.inPoint);
}

} // namespace led
