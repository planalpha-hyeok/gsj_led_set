#pragma once

// 소리 디코딩 (소리 파일 또는 영상 파일 속 소리)
// 별도 스레드에서 48kHz 스테레오 float로 변환해 버퍼에 쌓음. 소리 출력기가 꺼내 씀

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace led {

class AudioSource {
public:
    static constexpr int kSampleRate = 48000;
    static constexpr int kChannels = 2;

    // startTime: 파일의 이 시각(초)부터. loop: 끝나면 startTime으로 돌아가 반복
    AudioSource(std::string path, double startTime, bool loop);
    ~AudioSource();

    AudioSource(const AudioSource&) = delete;
    AudioSource& operator=(const AudioSource&) = delete;

    // 소리 스트림이 없으면 false (error = "소리 없음")
    bool open(std::string& error);
    void stop();

    // 최대 frames개(스테레오 1쌍 = 1개)를 out에 채움. 실제로 채운 수 반환 (부족하면 나머지는 호출한 쪽이 무음 처리)
    std::size_t read(float* out, std::size_t frames);
    // 재생 위치를 seconds만큼 앞으로 (버퍼에 있는 만큼만)
    void skip(double seconds);
    // 지금까지 꺼낸 양 (초)
    double position() const;
    bool finished() const;
    const std::string& path() const;

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace led
