#pragma once

// 타임라인 장면 재생
//  - 타임라인 시계(일시정지/이동 가능)에 맞춰 클립을 열고 닫음 (시작 1.5초 전에 미리 열기)
//  - 큐 지점에 도달하면 멈추고 GO를 기다림
//  - 끝 동작: 마지막 화면 유지 / 반복 / 정지
//  - 소리: 클립 소리 크기, 페이드, 레이어 음소거 반영
// tick()/appendViews()/제어 함수는 재생 엔진의 잠금 안에서만 호출

#include "core/Project.h"
#include "media/AudioOutput.h"
#include "media/MediaInfo.h"
#include "playback/RegionView.h"
#include "playback/TimelineClock.h"

#include <future>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace led {

struct TimelineStatus {
    bool active = false;       // 타임라인 장면 재생 중
    double time = 0.0;
    double length = 0.0;
    bool running = false;
    bool finished = false;     // 끝 동작 '정지'로 끝남
    std::string waitingCue;    // 큐에서 대기 중이면 큐 이름
};

class TimelinePlayer {
public:
    TimelinePlayer(const Project& project, const Scene& scene);
    ~TimelinePlayer();

    TimelinePlayer(const TimelinePlayer&) = delete;
    TimelinePlayer& operator=(const TimelinePlayer&) = delete;

    // 0초에 보일 클립을 미리 열고 첫 프레임을 기다림
    void prepare(std::vector<std::string>& errors, int timeoutMs);
    void start();
    void tick();
    void stop();

    // 편집 반영: 바뀐 클립만 다시 엶
    void reload(const Project& project, const Scene& scene);

    void pause();
    void resume();      // 일시정지/큐 대기에서 이어서 재생 (GO)
    void seek(double t);

    TimelineStatus status() const;
    void appendViews(std::vector<RegionView>& out) const;

private:
    struct Opened {
        std::shared_ptr<MediaSource> video;
        std::shared_ptr<AudioSource> audio;
        std::string error;
    };
    struct Instance {
        std::string layerId;
        TimelineClip clip;
        std::shared_ptr<MediaSource> video;
        std::shared_ptr<AudioTrack> audio;
        double sourceZero = 0.0;
        std::uint64_t key = 0;
    };
    struct Pending {
        std::string layerId;
        TimelineClip clip;
        double sourceZero = 0.0;
        std::future<Opened> future;
    };

    const TimelineLayer* findLayer(const std::string& id) const;
    void openClip(const TimelineLayer& layer, const TimelineClip& clip, double t, bool wait);
    void closeInstance(Instance& inst);
    void clearAll();
    void updateAudio(double t);
    bool layerShowsVideo(const TimelineLayer& layer) const;

    Timeline m_timeline;
    std::map<std::string, Region> m_regions;     // id -> 영역
    bool m_hardwareDefault = true;
    std::map<std::string, bool> m_regionHardware;

    std::shared_ptr<TimelineClock> m_clock = std::make_shared<TimelineClock>();
    std::map<std::string, Instance> m_instances; // clipId -> 재생 중
    std::map<std::string, Pending> m_pending;    // clipId -> 여는 중
    double m_lastTime = 0.0;
    bool m_finished = false;
    std::string m_waitingCue;
    bool m_audioReady = false;
};

} // namespace led
