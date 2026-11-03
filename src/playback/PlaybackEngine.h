#pragma once

// 재생 엔진: 영역별 소스를 관리하고, 공통 시계로 동기화된 장면을 출력에 제공

#include "core/Project.h"
#include "media/MediaSource.h"
#include "render/Scene.h"

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace led {

class PlaybackEngine final : public SceneProvider {
public:
    using Clock = std::chrono::steady_clock;

    PlaybackEngine() = default;
    ~PlaybackEngine() override;

    PlaybackEngine(const PlaybackEngine&) = delete;
    PlaybackEngine& operator=(const PlaybackEngine&) = delete;

    // 모든 소스를 열고 첫 프레임이 준비되면 동시에 재생 시작
    // errors: 열기에 실패한 영역 목록 (나머지는 재생됨)
    void play(const Project& project, std::vector<std::string>& errors);

    // 재생 중 영역 설정 변경 반영. 콘텐츠가 같은 영역은 재생을 이어감
    void update(const Project& project, std::vector<std::string>& errors);

    void stop();
    bool isPlaying() const;

    void collect(std::vector<SceneLayer>& out) override;

private:
    struct Layer {
        std::string regionId;
        std::uint64_t key = 0;
        RectI rect;
        FitMode fit = FitMode::Stretch;
        float opacity = 1.0f;
        bool loop = true;
        std::shared_ptr<MediaSource> source;
        Clock::time_point start;
    };
    using LayerList = std::vector<Layer>;

    std::shared_ptr<LayerList> buildLayers(const Project& project, const LayerList* previous,
                                           Clock::time_point now, std::vector<std::string>& errors,
                                           std::vector<std::shared_ptr<MediaSource>>& created);
    static void stopUnused(const LayerList* previous, const LayerList& next);

    mutable std::mutex m_mutex;
    std::shared_ptr<const LayerList> m_layers;
    bool m_playing = false;
};

} // namespace led
