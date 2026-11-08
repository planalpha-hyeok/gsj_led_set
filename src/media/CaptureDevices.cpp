#include "media/CaptureDevices.h"

extern "C" {
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
}

#include <mutex>

namespace led {

const char* captureInputFormat()
{
#if defined(_WIN32)
    return "dshow";
#elif defined(__APPLE__)
    return "avfoundation";
#else
    return "v4l2";
#endif
}

std::vector<CaptureDevice> listCaptureDevices(std::string* error)
{
    static std::once_flag flag;
    std::call_once(flag, [] { avdevice_register_all(); });

    std::vector<CaptureDevice> out;
    const AVInputFormat* fmt = av_find_input_format(captureInputFormat());
    if (!fmt) {
        if (error)
            *error = std::string("라이브 입력 방식을 지원하지 않음: ") + captureInputFormat();
        return out;
    }
    AVDeviceInfoList* list = nullptr;
    const int r = avdevice_list_input_sources(fmt, nullptr, nullptr, &list);
    if (r < 0 || !list) {
        if (error) {
            char buf[AV_ERROR_MAX_STRING_SIZE] = {0};
            av_strerror(r, buf, sizeof(buf));
            *error = std::string("입력 장치 목록을 가져올 수 없음 (") + buf + ")";
        }
        avdevice_free_list_devices(&list);
        return out;
    }
    for (int i = 0; i < list->nb_devices; ++i) {
        const AVDeviceInfo* d = list->devices[i];
        if (!d || !d->device_name)
            continue;
#if LIBAVDEVICE_VERSION_MAJOR >= 59
        // 영상 장치만 (오디오 장치 제외)
        bool video = d->nb_media_types == 0;
        for (int m = 0; m < d->nb_media_types; ++m)
            if (d->media_types[m] == AVMEDIA_TYPE_VIDEO)
                video = true;
        if (!video)
            continue;
#endif
        CaptureDevice dev;
        dev.name = d->device_description ? d->device_description : d->device_name;
        // DirectShow는 표시 이름으로 여는 것이 가장 호환성이 높음
        dev.id = std::string(captureInputFormat()) == "dshow" ? dev.name : std::string(d->device_name);
        out.push_back(std::move(dev));
    }
    avdevice_free_list_devices(&list);
    return out;
}

} // namespace led
