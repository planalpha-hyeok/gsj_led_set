#pragma once

// OSC(Open Sound Control) 패킷 해석 (표준 C++만 사용)
// 지원 형식: i(int32) f(float32) s/S(문자열) h(int64) d(double) T F N, 번들(#bundle)

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace led {

struct OscMessage {
    std::string address;              // 예: "/scene"
    std::vector<std::string> args;    // 모든 인수를 문자열로 변환
};

// 잘못된 부분은 건너뜀. 해석된 메시지만 반환
std::vector<OscMessage> parseOscPacket(const std::uint8_t* data, std::size_t size);

// OSC 메시지를 제어 명령 문장으로 변환
//   "/scene" ["오전 장면"]  ->  scene "오전 장면"
//   "/text" ["하단", "안녕"] ->  text "하단" "안녕"
std::string oscToCommand(const OscMessage& message);

} // namespace led
