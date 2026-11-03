#pragma once

// 영역 1개의 흐르는 자막 재생
//  - 자막 파일이 지정되면 1초마다 변경 여부를 확인해 바뀌면 새 내용으로 처음부터 다시 흐름
// tick()과 view()는 재생 제어 스레드에서만 호출

#include "playback/RegionView.h"

#include <QDateTime>

#include <string>
#include <vector>

namespace led {

class TickerPlayer {
public:
    TickerPlayer(RegionContent content, int regionWidth, int regionHeight);

    bool prepare(std::vector<std::string>& errors);
    void start(PlaybackClock::time_point startTime);
    void tick(PlaybackClock::time_point now);
    void setRegionSize(int width, int height, PlaybackClock::time_point now);

    std::shared_ptr<const TickerView> view() const { return m_view; }
    const RegionContent& content() const { return m_content; }

private:
    bool rebuild(std::string* error);
    void publish();

    RegionContent m_content;
    int m_width;
    int m_height;
    std::shared_ptr<const TickerStrip> m_strip;
    std::shared_ptr<const Frame> m_background;
    std::shared_ptr<const TickerView> m_view;
    PlaybackClock::time_point m_start;
    PlaybackClock::time_point m_lastFileCheck;
    QDateTime m_fileModified;
    qint64 m_fileSize = -1;
    std::uint64_t m_key = 0;
};

} // namespace led
