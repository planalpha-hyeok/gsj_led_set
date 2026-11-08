// 영상 디코딩, 시간 동기화, 반복 재생 검사
// 사용법: test_video <영상 파일>  (1초, 30fps 테스트 영상 기준)

#include "TestUtil.h"

#include "media/MediaFactory.h"
#include "media/VideoSource.h"

#include <chrono>
#include <cstdio>
#include <thread>

using namespace led;

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::fprintf(stderr, "영상 파일 경로가 필요함\n");
        return 2;
    }

    {
        VideoSource source(argv[1], true);
        std::string error;
        CHECK(source.open(error));
        CHECK(error.empty());
        CHECK(source.waitForFirstFrame(3000));

        auto first = source.frameAt(0.0);
        CHECK(first != nullptr);
        if (first) {
            CHECK(first->width == 320 && first->height == 240);
            CHECK(first->stride >= first->width * 4);
            CHECK(first->pixels.size() == static_cast<std::size_t>(first->stride) * first->height);
            CHECK(first->premultiplied);
        }

        // 시간이 지나면 다른 프레임이 나와야 함
        std::shared_ptr<const Frame> later;
        for (int i = 0; i < 100 && (!later || later == first); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            later = source.frameAt(0.5);
        }
        CHECK(later && later != first);
        if (later && first)
            CHECK(later->serial > first->serial);

        // 반복 재생 + 따라잡기: 1초 영상에서 갑자기 2.5초를 요청하면
        // 늦은 프레임은 변환 없이 건너뛰고(droppedFrames 증가) 2.5초 시점까지 도달해야 함
        std::shared_ptr<const Frame> looped;
        const std::uint64_t droppedBefore = source.droppedFrames();
        for (int i = 0; i < 300; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            looped = source.frameAt(2.5);
            if (looped && later && looped != later && source.droppedFrames() > droppedBefore + 20)
                break;
        }
        CHECK(looped && later && looped != later);
        CHECK(source.droppedFrames() > droppedBefore + 20);
        CHECK(source.decoderName() == "CPU");
        source.stop();
        source.stop(); // 두 번 호출해도 안전해야 함
    }

    {
        // 반복 끔: 끝난 뒤에도 마지막 프레임 유지
        VideoSource source(argv[1], false);
        std::string error;
        CHECK(source.open(error));
        std::shared_ptr<const Frame> last;
        for (int i = 0; i < 300; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            last = source.frameAt(100.0);
        }
        CHECK(last != nullptr);
        CHECK(source.frameAt(200.0) == last);
    }

    {
        // 하드웨어 디코딩 요청: 장치가 없거나 지원하지 않으면 CPU로 자동 전환되어 정상 재생
        VideoSource source(argv[1], true, true);
        std::string error;
        CHECK(source.open(error));
        CHECK(source.waitForFirstFrame(3000));
        auto f = source.frameAt(0.0);
        CHECK(f && f->width == 320 && f->height == 240);
        std::printf("하드웨어 요청 시 디코더: %s\n", source.decoderName().c_str());
        std::shared_ptr<const Frame> next;
        for (int i = 0; i < 100 && (!next || next == f); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            next = source.frameAt(0.5);
        }
        CHECK(next && next != f);
    }

    {
        // 라이브 입력 (시험 신호): 최신 프레임이 계속 바뀌어야 함
        auto live = createLiveSource("lavfi:testsrc2=size=160x120:rate=30", "", 0.0);
        std::string error;
        CHECK(live->open(error));
        CHECK(live->waitForFirstFrame(3000));
        auto a = live->frameAt(0.0);
        CHECK(a && a->width == 160 && a->height == 120);
        std::shared_ptr<const Frame> b;
        for (int i = 0; i < 100 && (!b || b == a); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            b = live->frameAt(0.0); // 라이브는 시각과 무관하게 최신 프레임
        }
        CHECK(b && b != a);
        CHECK(live->durationHint() < 0.0);
        live->stop();
    }

    {
        // 라이브 입력 끊김 -> 재연결: 1초짜리 신호가 끝나도 2초 뒤 다시 열려 프레임이 이어져야 함
        auto live = createLiveSource("lavfi:testsrc2=size=160x120:rate=30:duration=1", "", 0.0);
        std::string error;
        CHECK(live->open(error));
        CHECK(live->waitForFirstFrame(3000));
        std::this_thread::sleep_for(std::chrono::milliseconds(1500)); // 첫 신호 종료
        auto before = live->frameAt(0.0);
        std::shared_ptr<const Frame> after;
        for (int i = 0; i < 400 && (!after || after == before); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            after = live->frameAt(0.0);
        }
        CHECK(before && after && after != before);
        live->stop(); // 재연결 대기 중에도 바로 멈춰야 함
    }

    {
        // 없는 라이브 장치
        auto live = createLiveSource("이런장치없음", "", 0.0);
        std::string error;
        CHECK(!live->open(error));
        CHECK(!error.empty());
    }

    {
        // 없는 파일
        VideoSource source("__no_such_file__.mp4", true);
        std::string error;
        CHECK(!source.open(error));
        CHECK(!error.empty());
    }

    return test::finish("test_video");
}
