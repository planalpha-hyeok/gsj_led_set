#include "media/AudioOutput.h"

#include "core/Log.h"

#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace led {

namespace {

constexpr double kResyncSeconds = 0.12;

void dataCallback(ma_device* device, void* output, const void*, ma_uint32 frameCount)
{
    auto* self = static_cast<AudioOutput*>(device->pUserData);
    self->mix(static_cast<float*>(output), frameCount);
}

} // namespace

AudioOutput& AudioOutput::instance()
{
    static AudioOutput output;
    return output;
}

AudioOutput::~AudioOutput()
{
    shutdown();
}

bool AudioOutput::ensureContextLocked()
{
    if (m_context)
        return true;
    auto* ctx = new ma_context;
    if (ma_context_init(nullptr, 0, nullptr, ctx) != MA_SUCCESS) {
        delete ctx;
        return false;
    }
    m_context = ctx;
    return true;
}

bool AudioOutput::openDeviceLocked(std::string* error)
{
    if (!ensureContextLocked()) {
        m_failed = true;
        logWarning("소리 시스템을 초기화할 수 없음: 소리 없이 재생합니다");
        if (error)
            *error = "소리 시스템을 초기화할 수 없음";
        return false;
    }
    auto* ctx = static_cast<ma_context*>(m_context);

    // 지정한 장치 찾기 (이름 일치)
    ma_device_id chosenId;
    bool found = false;
    if (!m_deviceName.empty()) {
        ma_device_info* playback = nullptr;
        ma_uint32 count = 0;
        if (ma_context_get_devices(ctx, &playback, &count, nullptr, nullptr) == MA_SUCCESS) {
            for (ma_uint32 i = 0; i < count; ++i) {
                if (m_deviceName == playback[i].name) {
                    chosenId = playback[i].id;
                    found = true;
                    break;
                }
            }
        }
        if (!found)
            logWarning("소리 장치 '" + m_deviceName + "'를 찾을 수 없어 기본 장치로 출력합니다");
    }

    auto* device = new ma_device;
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = AudioSource::kChannels;
    config.playback.pDeviceID = found ? &chosenId : nullptr;
    config.sampleRate = AudioSource::kSampleRate;
    config.periodSizeInMilliseconds = 10;
    config.dataCallback = &dataCallback;
    config.pUserData = this;
    bool ok = ma_device_init(ctx, &config, device) == MA_SUCCESS;
    if (!ok) {
        // 대체: 모든 소리 방식을 차례로 시도해 기본 장치로 출력
        logWarning("선택한 방식으로 소리 장치를 열 수 없어 기본 장치로 다시 시도합니다");
        config.playback.pDeviceID = nullptr;
        ok = ma_device_init(nullptr, &config, device) == MA_SUCCESS;
    }
    if (!ok) {
        delete device;
        m_failed = true;
        logWarning("소리 장치를 열 수 없음: 소리 없이 재생합니다");
        if (error)
            *error = "소리 장치를 열 수 없음";
        return false;
    }
    if (ma_device_start(device) != MA_SUCCESS) {
        ma_device_uninit(device);
        delete device;
        m_failed = true;
        logWarning("소리 장치를 시작할 수 없음: 소리 없이 재생합니다");
        if (error)
            *error = "소리 장치를 시작할 수 없음";
        return false;
    }
    m_device = device;
    m_started = true;
    m_failed = false;
    logInfo(std::string("소리 출력 시작: ") + device->playback.name + " (" +
            ma_get_backend_name(device->pContext->backend) + ")");
    return true;
}

void AudioOutput::closeDeviceLocked()
{
    auto* device = static_cast<ma_device*>(m_device);
    m_device = nullptr;
    m_started = false;
    if (device) {
        ma_device_uninit(device); // 진행 중인 소리 콜백이 끝날 때까지 기다림
        delete device;
    }
}

bool AudioOutput::ensureStarted(std::string* error)
{
    std::lock_guard<std::mutex> lock(m_deviceMutex);
    if (m_started)
        return true;
    if (m_failed) {
        if (error)
            *error = "소리 장치를 사용할 수 없음";
        return false;
    }
    return openDeviceLocked(error);
}

std::vector<AudioDeviceInfo> AudioOutput::devices(std::string* error)
{
    std::lock_guard<std::mutex> lock(m_deviceMutex);
    std::vector<AudioDeviceInfo> out;
    if (!ensureContextLocked()) {
        if (error)
            *error = "소리 시스템을 초기화할 수 없음";
        return out;
    }
    ma_device_info* playback = nullptr;
    ma_uint32 count = 0;
    if (ma_context_get_devices(static_cast<ma_context*>(m_context), &playback, &count, nullptr, nullptr) !=
        MA_SUCCESS) {
        if (error)
            *error = "소리 장치 목록을 가져올 수 없음";
        return out;
    }
    for (ma_uint32 i = 0; i < count; ++i)
        out.push_back(AudioDeviceInfo{playback[i].name, playback[i].isDefault != 0});
    return out;
}

void AudioOutput::setDevice(const std::string& name)
{
    std::lock_guard<std::mutex> lock(m_deviceMutex);
    if (name == m_deviceName)
        return;
    m_deviceName = name;
    m_failed = false; // 다른 장치는 다시 시도
    if (m_started) {
        // 재생 중이면 새 장치로 바꿈 (트랙은 그대로 이어서 재생)
        closeDeviceLocked();
        openDeviceLocked(nullptr);
    }
}

std::string AudioOutput::requestedDevice() const
{
    std::lock_guard<std::mutex> lock(m_deviceMutex);
    return m_deviceName;
}

std::string AudioOutput::currentDeviceName() const
{
    std::lock_guard<std::mutex> lock(m_deviceMutex);
    const auto* device = static_cast<const ma_device*>(m_device);
    return device ? std::string(device->playback.name) : std::string();
}

void AudioOutput::shutdown()
{
    std::lock_guard<std::mutex> lock(m_deviceMutex);
    closeDeviceLocked();
    {
        std::lock_guard<std::mutex> tracks(m_mutex);
        m_tracks.clear();
    }
    if (m_context) {
        auto* ctx = static_cast<ma_context*>(m_context);
        ma_context_uninit(ctx);
        delete ctx;
        m_context = nullptr;
    }
}

void AudioOutput::add(const std::shared_ptr<AudioTrack>& track)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_tracks.push_back(track);
}

void AudioOutput::remove(const std::shared_ptr<AudioTrack>& track)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_tracks.erase(std::remove(m_tracks.begin(), m_tracks.end(), track), m_tracks.end());
}

std::size_t AudioOutput::trackCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_tracks.size();
}

void AudioOutput::mix(float* out, std::size_t frames)
{
    const std::size_t samples = frames * AudioSource::kChannels;
    std::memset(out, 0, samples * sizeof(float));

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_scratch.size() < samples)
        m_scratch.resize(samples);
    float* buf = m_scratch.data();

    for (const auto& trackPtr : m_tracks) {
        AudioTrack& track = *trackPtr;
        if (!track.running.load())
            continue;
        AudioSource& source = track.source();

        // 재생 위치 맞춤 (늦으면 건너뛰고, 빠르면 무음 삽입)
        const double diff = track.expected.load() - source.position();
        if (diff > kResyncSeconds)
            source.skip(diff);
        else if (diff < -kResyncSeconds && track.silenceFrames == 0)
            track.silenceFrames = static_cast<std::uint64_t>(-diff * AudioSource::kSampleRate);

        std::size_t offset = 0;
        if (track.silenceFrames > 0) {
            offset = static_cast<std::size_t>(std::min<std::uint64_t>(track.silenceFrames, frames));
            track.silenceFrames -= offset;
        }
        const std::size_t got = offset < frames ? source.read(buf, frames - offset) : 0;

        // 소리 크기 변화는 이번 구간 동안 부드럽게 (딸깍 소리 방지)
        const float target = std::clamp(track.gain.load(), 0.0f, 4.0f);
        const float start = track.currentGain;
        const float step = got > 0 ? (target - start) / static_cast<float>(got) : 0.0f;
        float g = start;
        for (std::size_t i = 0; i < got; ++i) {
            g += step;
            float* dst = out + (offset + i) * AudioSource::kChannels;
            const float* src = buf + i * AudioSource::kChannels;
            dst[0] += src[0] * g;
            dst[1] += src[1] * g;
        }
        track.currentGain = target;
    }

    for (std::size_t i = 0; i < samples; ++i)
        out[i] = std::clamp(out[i], -1.0f, 1.0f);
}

} // namespace led
