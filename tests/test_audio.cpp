// 소리 검사: 디코딩, 시작 위치(자르기), 반복, 소리 없는 파일, 출력기 섞기
// 사용법: test_audio <소리 파일(1초, 440Hz 앞 0.5초 + 무음 0.5초)> <소리 없는 영상>

#include "TestUtil.h"

#include "media/AudioOutput.h"
#include "media/AudioSource.h"
#include "media/MediaInfo.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <thread>
#include <vector>

using namespace led;

static double rms(const std::vector<float>& v, std::size_t from, std::size_t to)
{
    double s = 0;
    std::size_t n = 0;
    for (std::size_t i = from * 2; i < to * 2 && i < v.size(); ++i, ++n)
        s += double(v[i]) * v[i];
    return n ? std::sqrt(s / n) : 0.0;
}

static std::vector<float> readAll(AudioSource& src, std::size_t frames)
{
    std::vector<float> out(frames * 2, 0.0f);
    std::size_t done = 0;
    for (int tries = 0; tries < 300 && done < frames; ++tries) {
        done += src.read(out.data() + done * 2, frames - done);
        if (done < frames)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    out.resize(done * 2);
    return out;
}

int main(int argc, char* argv[])
{
    if (argc < 3) {
        std::fprintf(stderr, "인수 필요\n");
        return 2;
    }
    const int rate = AudioSource::kSampleRate;

    {
        // 처음부터: 앞 0.5초 소리, 뒤 0.5초 무음
        AudioSource s(argv[1], 0.0, false);
        std::string error;
        CHECK(s.open(error));
        auto pcm = readAll(s, rate);
        CHECK(pcm.size() >= static_cast<std::size_t>(rate * 2 * 0.95));
        std::printf("RMS 소리 구간 %.4f, 무음 구간 %.4f\n", rms(pcm, 0, rate / 4), rms(pcm, rate * 6 / 10, rate * 9 / 10));
        CHECK(rms(pcm, 0, rate / 4) > 0.03); // 시험 신호 진폭 0.125
        CHECK(rms(pcm, rate * 6 / 10, rate * 9 / 10) < 0.005);
        CHECK(std::fabs(s.position() - double(pcm.size() / 2) / rate) < 1e-9);
    }
    {
        // 자르기: 0.5초부터 -> 처음부터 무음
        AudioSource s(argv[1], 0.5, false);
        std::string error;
        CHECK(s.open(error));
        auto pcm = readAll(s, rate / 4);
        CHECK(pcm.size() == static_cast<std::size_t>(rate / 4 * 2));
        CHECK(rms(pcm, 0, rate / 4) < 0.005);
    }
    {
        // 반복: 1초 파일을 2.2초 분량 읽으면 1.0~1.4초 구간에 다시 소리
        AudioSource s(argv[1], 0.0, true);
        std::string error;
        CHECK(s.open(error));
        auto pcm = readAll(s, rate * 22 / 10);
        CHECK(pcm.size() >= static_cast<std::size_t>(rate * 2 * 2));
        CHECK(rms(pcm, rate + rate / 20, rate + rate * 4 / 10) > 0.03);
        // 건너뛰기
        const double before = s.position();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        s.skip(0.1);
        CHECK(s.position() > before);
    }
    {
        // 소리 없는 영상
        AudioSource s(argv[2], 0.0, false);
        std::string error;
        CHECK(!s.open(error));
        CHECK(error == "소리 없음");
    }
    {
        // 파일 정보
        MediaInfo a = probeMedia(argv[1]);
        CHECK(a.ok && a.hasAudio && !a.hasVideo && std::fabs(a.duration - 1.0) < 0.1);
        MediaInfo v = probeMedia(argv[2]);
        CHECK(v.ok && v.hasVideo && !v.hasAudio && !v.still);
        MediaInfo bad = probeMedia("__없는파일__.wav");
        CHECK(!bad.ok);
    }
    {
        // 출력기 섞기: 장치 없이 mix()를 직접 호출해 검사
        auto src = std::make_shared<AudioSource>(argv[1], 0.0, false);
        std::string error;
        CHECK(src->open(error));
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        auto track = std::make_shared<AudioTrack>(src);
        track->gain = 0.5f;
        track->running = true;
        AudioOutput& out = AudioOutput::instance();
        out.add(track);
        std::vector<float> buf(480 * 2);
        out.mix(buf.data(), 480); // 첫 구간: 0 -> 0.5로 서서히
        out.mix(buf.data(), 480);
        double peak = 0;
        for (float f : buf)
            peak = std::max(peak, double(std::fabs(f)));
        std::printf("섞은 결과 최대값 %.4f (원본 0.125 x 0.5)\n", peak);
        CHECK(peak > 0.03 && peak <= 0.0625 + 1e-3); // 소리 크기 0.5 반영
        track->running = false;                   // 일시정지: 무음, 위치 그대로
        const double pos = src->position();
        out.mix(buf.data(), 480);
        CHECK(src->position() == pos);
        for (float f : buf)
            CHECK(f == 0.0f);
        out.remove(track);
        CHECK(out.trackCount() == 0);
        std::string devError;
        const bool started = out.ensureStarted(&devError);
        std::printf("소리 장치: %s\n", started ? "시작됨" : devError.c_str());
        // 장치 목록과 선택
        std::string listError;
        const auto list = out.devices(&listError);
        std::printf("소리 장치 %d개%s\n", static_cast<int>(list.size()), listError.c_str());
        for (const auto& d : list)
            std::printf("  - %s%s\n", d.name.c_str(), d.isDefault ? " (기본)" : "");
        if (started && !list.empty()) {
            out.setDevice(list.front().name); // 재생 중 장치 바꾸기
            CHECK(out.isStarted());
            CHECK(out.currentDeviceName() == list.front().name);
            CHECK(out.requestedDevice() == list.front().name);
        }
        out.setDevice("__없는장치__"); // 없으면 기본 장치로
        if (started)
            CHECK(out.isStarted() && !out.currentDeviceName().empty());
        out.setDevice("");
        CHECK(out.requestedDevice().empty());
        out.shutdown();
        CHECK(!out.isStarted());
    }
    return test::finish("test_audio");
}
