#pragma once

// 소리 출력 (miniaudio, Windows: WASAPI 기본 장치, 48kHz 스테레오)
// 여러 소리 트랙을 섞어 출력. 각 트랙의 소리 크기/재생 여부/기대 위치는 재생 엔진이 갱신

#include "media/AudioSource.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace led {

class AudioTrack {
public:
    explicit AudioTrack(std::shared_ptr<AudioSource> source)
        : m_source(std::move(source))
    {
    }

    AudioSource& source() { return *m_source; }

    // 재생 엔진이 갱신 (어느 스레드에서든)
    std::atomic<float> gain{0.0f};         // 0 ~ 1 (페이드/소리 크기/음소거 반영)
    std::atomic<bool> running{false};      // false면 무음 + 위치 정지 (일시정지)
    std::atomic<double> expected{0.0};     // 지금 재생되어야 할 위치(초). 0.12초 이상 어긋나면 맞춤

    // 출력기 내부용
    float currentGain = 0.0f;
    std::uint64_t silenceFrames = 0;

private:
    std::shared_ptr<AudioSource> m_source;
};

struct AudioDeviceInfo {
    std::string name;
    bool isDefault = false;
};

class AudioOutput {
public:
    static AudioOutput& instance();

    // 처음 호출 시 소리 장치를 엶. 실패해도 프로그램은 계속 동작 (소리만 없음)
    bool ensureStarted(std::string* error = nullptr);
    bool isStarted() const { return m_started; }

    // 소리 출력 장치 목록
    std::vector<AudioDeviceInfo> devices(std::string* error = nullptr);
    // 출력 장치 지정 (빈 문자열 = 기본 장치). 재생 중이면 바로 새 장치로 바꿈
    // 지정한 장치가 없으면 기본 장치로 출력
    void setDevice(const std::string& name);
    std::string requestedDevice() const;
    std::string currentDeviceName() const;

    void add(const std::shared_ptr<AudioTrack>& track);
    void remove(const std::shared_ptr<AudioTrack>& track);
    std::size_t trackCount() const;

    void shutdown();

    // 소리 장치 콜백에서 호출 (내부용)
    void mix(float* out, std::size_t frames);

private:
    AudioOutput() = default;
    ~AudioOutput();

    bool ensureContextLocked();
    bool openDeviceLocked(std::string* error);
    void closeDeviceLocked();

    // 섞기(소리 콜백)용: 트랙 목록
    mutable std::mutex m_mutex;
    std::vector<std::shared_ptr<AudioTrack>> m_tracks;
    std::vector<float> m_scratch;

    // 장치 열기/닫기용 (소리 콜백에서는 쓰지 않음)
    mutable std::mutex m_deviceMutex;
    void* m_context = nullptr;  // ma_context*
    void* m_device = nullptr;   // ma_device*
    std::string m_deviceName;   // 요청한 장치 (빈 문자열 = 기본)
    std::atomic<bool> m_started{false};
    bool m_failed = false;
};

} // namespace led
