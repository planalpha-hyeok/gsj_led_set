#pragma once

// 장면(타임라인) 재생
//  - 타임라인 시계(일시정지/이동 가능)에 맞춰 클립을 열고 닫음 (시작 1.5초 전에 미리 열기)
//  - 레이어 반복: 레이어 길이만큼 계속 반복 (플레이리스트). 클립 1개만 있으면 끊김 없이 계속 재생
//  - 마지막 화면 유지: 반복하지 않는 레이어의 마지막 클립 화면을 계속 표시
//  - 큐 지점에 도달하면 멈추고 GO를 기다림
//  - 끝 동작: 마지막 화면 유지 / 반복 / 정지 (장면 길이 0 = 끝없이 재생)
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
#include <set>
#include <string>
#include <vector>

namespace led {

class TickerPlayer;

struct TimelineStatus {
    bool active = false;       // 장면 재생 중
    double time = 0.0;
    double length = 0.0;       // 0 = 끝없이 재생
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

    // 영역의 플레이리스트 레이어에서 다음(+1)/이전(-1) 클립으로 이동. 해당 레이어가 없으면 false
    bool skip(const std::string& regionId, int delta);

    TimelineStatus status() const;
    void appendViews(std::vector<RegionView>& out) const;

private:
    // 시각 t에 필요한 클립 1회 재생분 (반복 레이어는 바퀴마다 따로)
    struct Occurrence {
        const TimelineLayer* layer = nullptr;
        const TimelineClip* clip = nullptr;
        std::string key;          // 클립 id + 바퀴 번호
        double start = 0.0;       // 타임라인 시각
        double showEnd = 0.0;     // 화면 표시 끝 (계속 표시 = kForever)
        double soundEnd = 0.0;    // 소리 끝
        bool continuous = false;  // 끊김 없이 계속 (파일 반복)
    };
    struct Opened {
        std::shared_ptr<MediaSource> video;
        std::shared_ptr<AudioSource> audio;
        std::string error;
    };
    struct Instance {
        std::string layerId;
        TimelineClip clip;
        double start = 0.0;
        double showEnd = 0.0;
        double soundEnd = 0.0;
        std::shared_ptr<MediaSource> video;
        std::shared_ptr<AudioTrack> audio;
        std::shared_ptr<TickerPlayer> ticker;
        double sourceZero = 0.0;
        std::uint64_t key = 0;
    };
    struct Pending {
        Instance inst;
        std::future<Opened> future;
    };

    void collectOccurrences(double t, double preload, std::vector<Occurrence>& out) const;
    const TimelineLayer* findLayer(const std::string& id) const;
    void openOccurrence(const Occurrence& occ, double t, bool wait, std::vector<std::string>* errors);
    void closeInstance(Instance& inst);
    void closeLayer(const std::string& layerId);
    void clearAll();
    void updateAudio(double t);
    bool layerShowsVideo(const TimelineLayer& layer) const;
    void setRegions(const Project& project);

    Timeline m_timeline;
    std::vector<Region> m_regionOrder;           // 그리는 순서 (뒤 -> 앞)
    std::map<std::string, Region> m_regions;     // id -> 영역
    bool m_hardwareDefault = true;
    std::map<std::string, bool> m_regionHardware;

    std::shared_ptr<TimelineClock> m_clock = std::make_shared<TimelineClock>();
    std::map<std::string, Instance> m_instances; // 재생 중 (Occurrence::key)
    std::map<std::string, Pending> m_pending;    // 여는 중
    std::map<std::string, double> m_layerOffset; // 다음/이전 이동으로 생긴 레이어 시간 이동 (이동/처음부터 재생 시 초기화)
    double m_lastTime = 0.0;
    bool m_finished = false;
    std::string m_waitingCue;
    bool m_audioReady = false;
    std::vector<std::string>* m_errorSink = nullptr;  // prepare() 동안 열기 실패를 모음
    std::set<std::string> m_failedSources;            // 이미 알린 열기 실패 (반복 레이어에서 매번 알리지 않음)
};

} // namespace led
