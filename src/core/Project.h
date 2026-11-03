#pragma once

// 프로젝트 데이터 모델
//   캔버스 / 영역(위치) / 장면(영역별 콘텐츠) / 스케줄 / 출력
// 문자열은 모두 UTF-8 std::string

#include "core/Geometry.h"

#include <cstdint>
#include <string>
#include <vector>

namespace led {

struct CanvasConfig {
    int width = 1920;
    int height = 1080;
};

// 캔버스 위의 재생 영역 (위치와 표시 방식). 내용은 장면마다 따로 지정
struct Region {
    std::string id;
    std::string name;
    RectI rect;              // 캔버스 좌표 (LED 매핑 좌표와 동일한 픽셀)
    FitMode fit = FitMode::Stretch;
    double opacity = 1.0;    // 0.0 ~ 1.0
    bool visible = true;
};

// ---------------------------------------------------------------- 플레이리스트

enum class TransitionType { Cut, Fade };

struct PlaylistItem {
    std::string id;
    std::string source;              // 영상/이미지 파일 경로
    double duration = 0.0;           // 초. 0 = 자동 (영상: 끝까지 1회, 이미지: 10초)
    TransitionType transition = TransitionType::Cut;  // 이 항목이 시작될 때 전환 방식
    double transitionDuration = 1.0; // 페이드 시간(초)

    bool operator==(const PlaylistItem& o) const;
    bool operator!=(const PlaylistItem& o) const { return !(*this == o); }
};

constexpr double kDefaultImageDuration = 10.0;

// ---------------------------------------------------------------- 흐르는 자막

enum class TickerDirection { Left, Right, Up, Down };

struct TickerConfig {
    std::vector<std::string> lines;     // 순서대로 흐르는 문구
    std::string textFile;               // 지정 시 이 파일 내용 사용 (줄마다 문구 1개, 변경 시 자동 반영)
    std::string fontFamily = "Malgun Gothic";
    int fontSize = 64;                  // 픽셀
    bool bold = true;
    std::uint32_t textColor = 0xFFFFFFFFu;        // ARGB
    std::uint32_t backgroundColor = 0x00000000u;  // ARGB (투명 = 뒤 영역이 보임)
    int outlineWidth = 0;                         // 0 = 외곽선 없음
    std::uint32_t outlineColor = 0xFF000000u;
    bool shadow = false;
    int shadowOffset = 4;
    std::uint32_t shadowColor = 0xB4000000u;
    double speed = 150.0;               // 픽셀/초
    TickerDirection direction = TickerDirection::Left;
    int gap = 200;                      // 문구 사이 간격(픽셀)

    bool operator==(const TickerConfig& o) const;
    bool operator!=(const TickerConfig& o) const { return !(*this == o); }
};

// ---------------------------------------------------------------- 장면

enum class ContentKind { Playlist, Ticker };

struct RegionContent {
    std::string regionId;
    ContentKind kind = ContentKind::Playlist;
    std::vector<PlaylistItem> items;
    bool loopPlaylist = true;           // 마지막 항목 후 처음부터 반복 (끄면 마지막 화면 유지)
    TickerConfig ticker;

    bool isEmpty() const;
    bool operator==(const RegionContent& o) const;
    bool operator!=(const RegionContent& o) const { return !(*this == o); }
};

struct Scene {
    std::string id;
    std::string name;
    std::vector<RegionContent> contents;

    RegionContent* contentFor(const std::string& regionId);
    const RegionContent* contentFor(const std::string& regionId) const;
};

// ---------------------------------------------------------------- 스케줄

struct ScheduleEntry {
    std::string id;
    std::string name;
    std::string sceneId;
    bool enabled = true;
    std::uint8_t weekdays = 0x7F;  // bit0 = 월 ... bit6 = 일
    std::string date;              // "YYYY-MM-DD" 지정 시 해당 날짜에만 적용 (요일보다 우선)
    int startMinute = 9 * 60;      // 0 ~ 1439
    int endMinute = 18 * 60;       // 시작보다 작으면 자정을 넘김, 시작과 같으면 하루 종일
};

enum class OutsideBehavior {
    Black,         // 일정이 없는 시간: 재생 정지 (검은 화면)
    DefaultScene   // 일정이 없는 시간: 기본 장면 재생
};

struct Schedule {
    bool enabled = false;
    OutsideBehavior outside = OutsideBehavior::Black;
    std::string defaultSceneId;
    std::vector<ScheduleEntry> entries;
};

// ---------------------------------------------------------------- 출력

// 그래픽카드 출력 1개 = 출력 창 1개
struct OutputConfig {
    std::string id;
    std::string name;
    std::string displayId;   // Windows: "\\.\DISPLAY2" 형식
    RectI canvasRect;        // 이 출력이 내보낼 캔버스 범위 (출력 창 크기 = w x h, 1:1 픽셀)
    int offsetX = 0;         // 디스플레이 좌상단 기준 출력 창 위치
    int offsetY = 0;
    bool enabled = true;
};

// ---------------------------------------------------------------- 프로젝트

struct Project {
    CanvasConfig canvas;
    std::vector<Region> regions;      // 배열 순서 = 그리는 순서 (뒤쪽 요소가 앞에 보임)
    std::vector<Scene> scenes;        // 최소 1개
    Schedule schedule;
    std::vector<OutputConfig> outputs;
    bool autoStart = false;           // 프로그램 시작 시 출력 켜기 + 재생(또는 스케줄 운영)

    Region* findRegion(const std::string& id);
    const Region* findRegion(const std::string& id) const;
    OutputConfig* findOutput(const std::string& id);
    const OutputConfig* findOutput(const std::string& id) const;
    Scene* findScene(const std::string& id);
    const Scene* findScene(const std::string& id) const;
};

std::string generateId();

const char* fitModeKey(FitMode mode);
bool fitModeFromKey(const std::string& key, FitMode& out);
const char* transitionKey(TransitionType t);
bool transitionFromKey(const std::string& key, TransitionType& out);
const char* tickerDirectionKey(TickerDirection d);
bool tickerDirectionFromKey(const std::string& key, TickerDirection& out);

// "#AARRGGBB" <-> ARGB
std::string colorToString(std::uint32_t argb);
bool colorFromString(const std::string& text, std::uint32_t& out);

// "HH:MM" <-> 분
std::string minuteToString(int minute);
bool minuteFromString(const std::string& text, int& out);

} // namespace led
