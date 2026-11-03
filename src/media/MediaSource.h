#pragma once

// 재생 콘텐츠 공통 인터페이스
// frameAt()은 여러 출력 스레드에서 동시에 호출될 수 있음

#include "media/Frame.h"

#include <memory>
#include <string>

namespace led {

class MediaSource {
public:
    virtual ~MediaSource() = default;

    // 파일을 열고 디코딩 준비. 실패 시 error에 사유 기록
    virtual bool open(std::string& error) = 0;

    // 재생 시작 후 경과 시간(초)에 표시할 프레임. 아직 없으면 nullptr
    virtual std::shared_ptr<const Frame> frameAt(double seconds) = 0;

    // 첫 프레임이 준비될 때까지 대기 (동기화 시작용)
    virtual bool waitForFirstFrame(int timeoutMs) = 0;

    // 디코딩 중지. 여러 번 호출해도 안전
    virtual void stop() = 0;

    virtual const std::string& path() const = 0;

    // 정지 이미지 여부
    virtual bool isStill() const { return false; }

    // 파일 정보상의 재생 길이(초). 모르면 음수
    virtual double durationHint() const { return -1.0; }

    // 반복 없이 끝까지 재생했을 때 실제 끝난 시각(초). 아직 끝나지 않았으면 음수
    virtual double endTime() const { return -1.0; }
};

} // namespace led
