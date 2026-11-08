#pragma once

// 외부 제어 명령 해석 (표준 C++만 사용)
//
// 명령 (영문/한글 모두 가능, 대소문자 무관). 이름에 공백이 있으면 "따옴표"
//   scene <장면 이름|번호>        장면       장면 전환
//   play                           재생       현재(또는 편집 중인) 장면 재생
//   stop                           정지       정지
//   next <영역>                    다음       플레이리스트 다음 항목
//   prev <영역>                    이전       플레이리스트 이전 항목
//   text <영역> <문구>             자막       자막 문구 변경 ('|'로 여러 문구)
//   show <영역>                    표시       영역 표시
//   hide <영역>                    숨김       영역 숨김
//   trigger <이름>                 트리거     트리거 연결표의 명령 실행
//   reset                          초기화     외부 명령으로 바꾼 자막/표시 상태 되돌림
//   pause                          일시정지   타임라인 일시정지
//   go (resume)                    계속       타임라인 이어서 재생 / 큐에서 계속
//   seek <시각>                    이동       타임라인 위치 이동 (예: 90, 1:30, 1:02:03.5)

#include <string>
#include <vector>

namespace led {

enum class CommandType { Scene, Play, Stop, Next, Prev, Text, Show, Hide, Trigger, Reset, Pause, Go, Seek };

struct ControlCommand {
    CommandType type = CommandType::Play;
    std::string target;                // 장면/영역/트리거 이름
    std::vector<std::string> lines;    // text 명령의 문구들
    double seconds = 0.0;              // seek 명령의 시각
};

// "90", "1:30", "1:02:03.5" -> 초. 실패하면 false
bool parseTimeText(const std::string& text, double& seconds);
// 초 -> "m:ss.s" 또는 "h:mm:ss.s"
std::string formatTimeText(double seconds);

bool parseCommand(const std::string& line, ControlCommand& out, std::string& error);

// 앞뒤 공백 제거
std::string trimText(const std::string& s);

} // namespace led
