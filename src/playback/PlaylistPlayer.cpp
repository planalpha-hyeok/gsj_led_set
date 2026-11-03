#include "playback/PlaylistPlayer.h"

#include "core/Log.h"
#include "media/MediaFactory.h"

#include <algorithm>
#include <atomic>

namespace led {

std::uint64_t nextRenderKey()
{
    static std::atomic<std::uint64_t> counter{1};
    return counter++;
}

namespace {

std::shared_ptr<MediaSource> openSource(const std::string& path, bool loop, std::string& error)
{
    if (path.empty()) {
        error = "파일이 지정되지 않음";
        return nullptr;
    }
    std::shared_ptr<MediaSource> source = createMediaSource(path, loop);
    if (!source->open(error)) {
        source->stop();
        return nullptr;
    }
    return source;
}

PlaybackClock::duration seconds(double s)
{
    return std::chrono::duration_cast<PlaybackClock::duration>(std::chrono::duration<double>(s));
}

} // namespace

PlaylistPlayer::PlaylistPlayer(RegionContent content)
    : m_content(std::move(content))
{
}

PlaylistPlayer::~PlaylistPlayer()
{
    stop();
}

std::optional<std::size_t> PlaylistPlayer::nextIndex(std::size_t index) const
{
    const std::size_t n = m_content.items.size();
    if (n == 0)
        return std::nullopt;
    if (index + 1 < n)
        return index + 1;
    if (m_content.loopPlaylist)
        return 0;
    return std::nullopt;
}

bool PlaylistPlayer::sourceLoops(std::size_t index) const
{
    const std::size_t n = m_content.items.size();
    const PlaylistItem& item = m_content.items[index];
    const bool lastWithoutLoop = !m_content.loopPlaylist && index + 1 == n;
    if (n == 1 && m_content.loopPlaylist)
        return true;                 // 항목 1개 반복: 영상 자체를 반복
    if (item.duration > 0.0)
        return !lastWithoutLoop;     // 지정 시간을 채울 때까지 반복
    return false;                    // 자동: 끝까지 1회
}

double PlaylistPlayer::plannedDuration(const Run& run) const
{
    const PlaylistItem& item = m_content.items[run.index];
    if (item.duration > 0.0)
        return item.duration;
    if (run.source->isStill())
        return kDefaultImageDuration;
    const double end = run.source->endTime();
    if (end >= 0.0)
        return end;
    const double hint = run.source->durationHint();
    return hint > 0.0 ? hint : -1.0;
}

void PlaylistPlayer::startPreload(std::size_t index)
{
    m_preloadIndex = index;
    m_preloading = true;
    const std::string path = m_content.items[index].source;
    const bool loop = sourceLoops(index);
    m_preload = std::async(std::launch::async, [path, loop]() -> std::shared_ptr<MediaSource> {
        std::string error;
        std::shared_ptr<MediaSource> source = openSource(path, loop, error);
        if (!source)
            logError("플레이리스트 항목을 열 수 없음: " + error + " [" + path + "]");
        return source;
    });
}

bool PlaylistPlayer::prepare(std::vector<std::string>& errors)
{
    const std::size_t n = m_content.items.size();
    for (std::size_t i = 0; i < n; ++i) {
        std::string error;
        std::shared_ptr<MediaSource> source = openSource(m_content.items[i].source, sourceLoops(i), error);
        if (!source) {
            errors.push_back(error + " [" + m_content.items[i].source + "]");
            continue;
        }
        Run run;
        run.index = i;
        run.source = std::move(source);
        run.key = nextRenderKey();
        m_current = std::move(run);
        break;
    }
    if (!m_current)
        return false;

    m_static = n == 1 && m_content.loopPlaylist;
    if (!m_static) {
        if (const auto next = nextIndex(m_current->index))
            startPreload(*next);
    }
    return true;
}

bool PlaylistPlayer::waitForFirstFrame(int timeoutMs)
{
    return m_current && m_current->source->waitForFirstFrame(timeoutMs);
}

void PlaylistPlayer::start(PlaybackClock::time_point startTime)
{
    if (!m_current)
        return;
    m_current->start = startTime;
    m_current->fadeStart = startTime;
    m_current->fadeSeconds = 0.0;
}

void PlaylistPlayer::tick(PlaybackClock::time_point now)
{
    if (!m_current)
        return;

    // 페이드가 끝난 이전 항목 정리
    if (m_outgoing && now >= m_current->fadeStart + seconds(m_current->fadeSeconds)) {
        m_outgoing->source->stop();
        m_outgoing.reset();
    }
    if (m_static || m_outgoing)
        return;

    const auto next = nextIndex(m_current->index);
    if (!next)
        return; // 반복 끔: 마지막 항목 유지

    const double planned = plannedDuration(*m_current);
    if (planned < 0.0)
        return; // 길이를 아직 모름: 영상 끝을 기다림

    const PlaylistItem& upcoming = m_content.items[m_preloading ? m_preloadIndex : *next];
    double fade = upcoming.transition == TransitionType::Fade ? upcoming.transitionDuration : 0.0;
    fade = std::clamp(fade, 0.0, planned / 2.0);

    if (now < m_current->start + seconds(planned - fade))
        return;

    if (!m_preloading)
        startPreload(*next);
    if (m_preload.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return; // 아직 열리는 중 (현재 항목 유지)

    std::shared_ptr<MediaSource> source = m_preload.get();
    m_preloading = false;
    const std::size_t index = m_preloadIndex;

    if (!source) {
        // 열기 실패: 다음 항목으로 건너뜀. 모든 항목이 실패하면 현재 화면 유지
        if (++m_failCount >= m_content.items.size()) {
            logError("플레이리스트의 모든 다음 항목을 열 수 없어 현재 항목을 유지합니다");
            m_static = true;
            return;
        }
        if (const auto skip = nextIndex(index))
            startPreload(*skip);
        return;
    }
    m_failCount = 0;

    Run run;
    run.index = index;
    run.source = std::move(source);
    run.start = now;
    run.fadeStart = now;
    run.fadeSeconds = m_content.items[index].transition == TransitionType::Fade ? fade : 0.0;
    run.key = nextRenderKey();

    if (run.fadeSeconds > 0.0)
        m_outgoing = std::move(m_current);
    else
        m_current->source->stop();
    m_current = std::move(run);

    if (const auto following = nextIndex(index))
        startPreload(*following);
}

void PlaylistPlayer::stop()
{
    if (m_preloading && m_preload.valid()) {
        std::shared_ptr<MediaSource> pending = m_preload.get();
        if (pending)
            pending->stop();
    }
    m_preloading = false;
    if (m_outgoing)
        m_outgoing->source->stop();
    if (m_current)
        m_current->source->stop();
    m_outgoing.reset();
}

void PlaylistPlayer::appendClips(std::vector<ClipView>& out) const
{
    auto add = [&out](const Run& r) {
        ClipView c;
        c.source = r.source;
        c.start = r.start;
        c.fadeStart = r.fadeStart;
        c.fadeSeconds = r.fadeSeconds;
        c.key = r.key;
        out.push_back(std::move(c));
    };
    if (m_outgoing)
        add(*m_outgoing);
    if (m_current)
        add(*m_current);
}

} // namespace led
