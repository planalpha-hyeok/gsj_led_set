#pragma once

// 재생 엔진
//  - 장면(타임라인) 1개를 재생. 영역별 플레이리스트/자막은 그 영역의 반복 레이어
//  - 재생 제어 스레드(10ms 주기)가 클립 열기/닫기, 큐, 자막 파일 확인을 처리
//  - 출력 스레드는 collect()로 현재 시각에 맞는 프레임/투명도/자막 위치를 계산

#include "core/Project.h"
#include "playback/RegionView.h"
#include "playback/TimelinePlayer.h"
#include "render/Scene.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace led {

class PlaybackEngine final : public SceneProvider {
public:
    PlaybackEngine();
    ~PlaybackEngine() override;

    PlaybackEngine(const PlaybackEngine&) = delete;
    PlaybackEngine& operator=(const PlaybackEngine&) = delete;

    // 장면 재생 시작. 0초에 보일 모든 클립의 첫 프레임이 준비되면 동시에 시작
    // errors: 열 수 없는 콘텐츠 (나머지는 재생됨)
    void play(const Project& project, const std::string& sceneId, std::vector<std::string>& errors);

    // 재생 중 편집 반영. 바뀌지 않은 클립은 재생을 이어감
    void update(const Project& project, std::vector<std::string>& errors);

    void stop();
    bool isPlaying() const;

    // 영역 플레이리스트(그 영역의 첫 레이어)의 다음(+1)/이전(-1) 항목. 해당 레이어가 없으면 false
    bool skip(const std::string& regionId, int delta);

    // 장면 시계 제어 (재생 중이 아니면 false)
    bool timelinePause();
    bool timelineResume();          // 이어서 재생 / 큐에서 GO
    bool timelineSeek(double seconds);
    TimelineStatus timelineStatus() const;
    std::string sceneId() const;

    // 출력 화면 갱신 시점이 6ms 이내로 가까운 출력들은 같은 계산 결과(같은 프레임)를 공유
    void collect(std::vector<SceneLayer>& out) override;

private:
    void computeLayers(const std::vector<RegionView>& views, PlaybackClock::time_point now,
                       std::vector<SceneLayer>& out);
    void publishLocked();
    void controlLoop();
    void startControlThread();
    void stopControlThread();

    // 재생 상태 (제어 스레드와 GUI 스레드가 공유)
    mutable std::mutex m_slotMutex;
    std::unique_ptr<TimelinePlayer> m_timelinePlayer;
    std::string m_sceneId;
    bool m_playing = false;

    // 출력 스레드가 읽는 최신 화면 정보
    mutable std::mutex m_viewMutex;
    std::shared_ptr<const std::vector<RegionView>> m_views;

    // 출력 간 공유용 최근 계산 결과
    std::mutex m_shareMutex;
    std::shared_ptr<const std::vector<RegionView>> m_sharedViews;
    PlaybackClock::time_point m_sharedTime;
    std::vector<SceneLayer> m_sharedLayers;

    std::thread m_control;
    std::atomic<bool> m_controlStop{false};
    std::mutex m_wakeMutex;
    std::condition_variable m_wake;
};

} // namespace led
