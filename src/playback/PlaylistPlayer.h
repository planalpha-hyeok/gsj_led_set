#pragma once

// 영역 1개의 플레이리스트 재생
//  - 다음 항목을 미리 열어 두어 전환 시 끊김 방지
//  - 컷: 즉시 전환 / 페이드: 이전 항목 끝나기 전에 새 항목이 겹치며 서서히 나타남
// tick()과 appendClips()는 재생 제어 스레드에서만 호출

#include "playback/RegionView.h"

#include <future>
#include <optional>
#include <string>
#include <vector>

namespace led {

class PlaylistPlayer {
public:
    // hardwareDecoding: 영상 항목을 그래픽카드로 디코딩 (불가능하면 자동으로 CPU)
    PlaylistPlayer(RegionContent content, bool hardwareDecoding);
    ~PlaylistPlayer();

    PlaylistPlayer(const PlaylistPlayer&) = delete;
    PlaylistPlayer& operator=(const PlaylistPlayer&) = delete;

    // 첫 항목을 엶 (실패한 항목은 건너뜀). 열 수 있는 항목이 없으면 false
    bool prepare(std::vector<std::string>& errors);
    bool waitForFirstFrame(int timeoutMs);
    void start(PlaybackClock::time_point startTime);
    void tick(PlaybackClock::time_point now);
    // 외부 명령: +1 다음 항목, -1 이전 항목 (다음 tick에서 처리)
    void requestSkip(int delta);
    void stop();

    void appendClips(std::vector<ClipView>& out) const;
    const RegionContent& content() const { return m_content; }

private:
    struct Run {
        std::size_t index = 0;
        std::shared_ptr<MediaSource> source;
        PlaybackClock::time_point start;
        PlaybackClock::time_point fadeStart;
        double fadeSeconds = 0.0;
        std::uint64_t key = 0;
    };

    std::optional<std::size_t> nextIndex(std::size_t index) const;
    bool sourceLoops(std::size_t index) const;
    double plannedDuration(const Run& run) const;
    void startPreload(std::size_t index);
    void cancelPreload();

    RegionContent m_content;
    bool m_hardware = false;
    std::optional<Run> m_current;
    std::optional<Run> m_outgoing;
    std::future<std::shared_ptr<MediaSource>> m_preload;
    std::size_t m_preloadIndex = 0;
    bool m_preloading = false;
    bool m_static = false;          // 항목 1개 반복: 전환 없이 소스 자체 반복
    std::size_t m_failCount = 0;
    int m_skipRequest = 0;
    bool m_forcedSwitch = false;
};

} // namespace led
