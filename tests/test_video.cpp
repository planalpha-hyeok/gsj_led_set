// 영상 디코딩, 시간 동기화, 반복 재생 검사
// 사용법: test_video <영상 파일>  (1초, 30fps 테스트 영상 기준)

#include "TestUtil.h"

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

        // 반복 재생: 1초 영상에서 2.5초 시점 프레임이 나와야 함
        std::shared_ptr<const Frame> looped;
        for (int i = 0; i < 300; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            looped = source.frameAt(2.5);
            if (looped && later && looped->serial > later->serial + 40)
                break;
        }
        CHECK(looped && later && looped->serial > later->serial + 40);
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
        // 없는 파일
        VideoSource source("__no_such_file__.mp4", true);
        std::string error;
        CHECK(!source.open(error));
        CHECK(!error.empty());
    }

    return test::finish("test_video");
}
