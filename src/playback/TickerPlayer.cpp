#include "playback/TickerPlayer.h"

#include "core/Log.h"

#include <QFileInfo>

namespace led {

namespace {
constexpr auto kFileCheckInterval = std::chrono::seconds(1);
}

TickerPlayer::TickerPlayer(TickerConfig config, int regionWidth, int regionHeight)
    : m_config(std::move(config))
    , m_width(regionWidth)
    , m_height(regionHeight)
    , m_key(nextRenderKey())
{
    if ((m_config.backgroundColor >> 24) != 0)
        m_background = solidColorFrame(m_config.backgroundColor);
}

bool TickerPlayer::rebuild(std::string* error)
{
    const TickerConfig& cfg = m_config;
    if (!cfg.textFile.empty()) {
        const QFileInfo info(QString::fromStdString(cfg.textFile));
        m_fileModified = info.lastModified();
        m_fileSize = info.exists() ? info.size() : -1;
    }
    std::string readError;
    const std::vector<std::string> lines = tickerLines(cfg, &readError);
    if (!readError.empty()) {
        if (error)
            *error = readError;
        else
            logError(readError);
    }
    m_strip = buildTickerStrip(cfg, lines, m_width, m_height);
    // 새 띠마다 텍스처 번호를 바꿔 이전 이미지와 섞이지 않게 함
    m_key = nextRenderKey();
    return m_strip != nullptr;
}

bool TickerPlayer::prepare(std::vector<std::string>& errors)
{
    std::string error;
    rebuild(&error);
    if (!error.empty())
        errors.push_back(error);
    else if (!m_strip)
        errors.push_back("자막 문구가 비어 있음");
    // 파일이 아직 없거나 비어 있어도 재생은 시작 (파일이 생기면 자동 반영)
    return true;
}

void TickerPlayer::publish()
{
    auto view = std::make_shared<TickerView>();
    view->strip = m_strip;
    view->background = m_background;
    view->start = m_start;
    view->speed = m_config.speed;
    view->direction = m_config.direction;
    view->key = m_key;
    m_view = std::move(view);
}

void TickerPlayer::start(PlaybackClock::time_point startTime)
{
    m_start = startTime;
    m_lastFileCheck = startTime;
    publish();
}

void TickerPlayer::tick(PlaybackClock::time_point now)
{
    const std::string& file = m_config.textFile;
    if (file.empty() || now - m_lastFileCheck < kFileCheckInterval)
        return;
    m_lastFileCheck = now;

    const QFileInfo info(QString::fromStdString(file));
    const qint64 size = info.exists() ? info.size() : -1;
    if (size == m_fileSize && info.lastModified() == m_fileModified)
        return;

    logInfo("자막 파일 변경 감지: " + file);
    rebuild(nullptr);
    m_start = now; // 새 내용은 처음부터 흐름
    publish();
}

void TickerPlayer::setRegionSize(int width, int height, PlaybackClock::time_point now)
{
    if (width == m_width && height == m_height)
        return;
    m_width = width;
    m_height = height;
    rebuild(nullptr);
    m_start = now;
    publish();
}

} // namespace led
