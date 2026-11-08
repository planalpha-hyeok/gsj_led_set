#pragma once

// 캡처보드/웹캠 등 라이브 입력 장치 조회
// Windows: DirectShow, Linux: v4l2 (개발/검사용)

#include <string>
#include <vector>

namespace led {

struct CaptureDevice {
    std::string id;     // 입력 이름 (재생 시 사용)
    std::string name;   // 화면 표시용 이름
};

// 이 운영체제의 라이브 입력 방식 ("dshow", "v4l2" 등)
const char* captureInputFormat();

std::vector<CaptureDevice> listCaptureDevices(std::string* error = nullptr);

} // namespace led
