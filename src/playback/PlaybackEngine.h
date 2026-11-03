#pragma once

// 재생 엔진
//  - 장면 1개를 재생: 영역마다 플레이리스트 또는 흐르는 자막
//  - 재생 제어 스레드(10ms 주기)가 플레이리스트 전환/자막 파일 확인을 처리
//  - 출력 스레드는 collect()로 현재 시각에 맞는 프레임/투명도/자막 위치를 계산

#include "core/Project.h"
#include "playback/RegionView.h"
#include "render/Scene.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace led {

class PlaylistPlayer;
class TickerPlayer;

class PlaybackEngine final : public SceneProvider {
public:
    PlaybackEngine();
    ~PlaybackEngine() override;

    PlaybackEngine(const PlaybackEngine&) = delete;
    PlaybackEngine& operator=(const PlaybackEngine&) = delete;

    // 장면 재생 시작. 모든 영역의 첫 프레임이 준비되면 동시에 시작
    // errors: 열 수 없는 콘텐츠 (나머지는 재생됨)
    void play(const Project& project, const std::string& sceneId, std::vector<std::string>& errors);

    // 재생 중 편집 반영. 내용이 그대로인 영역은 재생을 이어감
    void update(const Project& project, std::vector<std::string>& errors);

    void stop();
    bool isPlaying() const;
    std::string sceneId() const;

    void collect(std::vector<SceneLayer>& out) override;

private:
    struct Slot;

    std::vector<std::unique_ptr<Slot>> buildSlots(const Project& project, const Scene& scene,
                                                  std::vector<std::unique_ptr<Slot>>* reuse,
                                                  std::vector<std::string>& errors,
                                                  std::vector<Slot*>& created);
    void publishLocked();
    void controlLoop();
    void startControlThread();
    void stopControlThread();

    // 재생 상태 (제어 스레드와 GUI 스레드가 공유)
    mutable std::mutex m_slotMutex;
    std::vector<std::unique_ptr<Slot>> m_slots;
    std::string m_sceneId;
    bool m_playing = false;

    // 출력 스레드가 읽는 최신 화면 정보
    mutable std::mutex m_viewMutex;
    std::shared_ptr<const std::vector<RegionView>> m_views;

    std::thread m_control;
    std::atomic<bool> m_controlStop{false};
    std::mutex m_wakeMutex;
    std::condition_variable m_wake;
};

} // namespace led
