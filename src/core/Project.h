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

// 영상 디코딩 방식
enum class DecodeMode {
    Default,   // 프로젝트 전체 설정 따름
    Hardware,  // 그래픽카드 (지원하지 않으면 자동으로 CPU)
    Software   // CPU
};

// 캔버스 위의 재생 영역 (위치와 표시 방식). 내용은 장면마다 따로 지정
struct Region {
    std::string id;
    std::string name;
    RectI rect;              // 캔버스 좌표 (LED 매핑 좌표와 동일한 픽셀)
    FitMode fit = FitMode::Stretch;
    double opacity = 1.0;    // 0.0 ~ 1.0
    bool visible = true;
    DecodeMode decode = DecodeMode::Default;
};

// ---------------------------------------------------------------- 플레이리스트

enum class TransitionType { Cut, Fade };

enum class ItemType {
    File,   // 영상/이미지 파일
    Live    // 캡처보드/웹캠 등 라이브 입력 장치
};

struct PlaylistItem {
    std::string id;
    ItemType type = ItemType::File;
    std::string source;              // 파일: 경로 / 라이브: 장치 이름
    std::string liveSize;            // 라이브: 입력 해상도 (예: "1920x1080", 비어 있으면 자동)
    double liveFps = 0.0;            // 라이브: 입력 프레임 (0 = 자동)
    double duration = 0.0;           // 초. 0 = 자동 (영상: 끝까지 1회, 이미지: 10초, 라이브: 계속)
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

// ---------------------------------------------------------------- 타임라인

// 시간축 위의 클립 1개 (영상/이미지/소리 파일)
struct TimelineClip {
    std::string id;
    std::string source;          // 파일 경로
    double start = 0.0;          // 타임라인 시작 시각(초)
    double duration = 5.0;       // 타임라인에서 차지하는 길이(초)
    double inPoint = 0.0;        // 자르기: 파일의 몇 초부터 사용
    double fadeIn = 0.0;         // 초
    double fadeOut = 0.0;        // 초
    double opacity = 1.0;        // 0 ~ 1
    double volume = 1.0;         // 0 ~ 1 (소리)
    bool loop = false;           // 파일이 클립보다 짧으면 자르기 지점부터 반복

    double end() const { return start + duration; }
    bool operator==(const TimelineClip& o) const;
    bool operator!=(const TimelineClip& o) const { return !(*this == o); }
};

struct TimelineLayer {
    std::string id;
    std::string name;
    std::string regionId;        // 표시할 영역 (소리만 있는 레이어는 비워도 됨)
    bool visible = true;         // 끄면 화면에 표시 안 함
    bool muted = false;          // 소리 끔
    bool locked = false;         // 편집 잠금
    std::vector<TimelineClip> clips;

    bool operator==(const TimelineLayer& o) const;
    bool operator!=(const TimelineLayer& o) const { return !(*this == o); }
};

// 큐 지점: 재생 중 이 시각에 도달하면 멈추고 GO 명령을 기다림
struct TimelineCue {
    std::string id;
    std::string name;
    double time = 0.0;

    bool operator==(const TimelineCue& o) const { return id == o.id && name == o.name && time == o.time; }
};

enum class TimelineEnd {
    Hold,   // 마지막 화면 유지
    Loop,   // 처음부터 반복
    Stop    // 정지 (검은 화면)
};

struct Timeline {
    std::vector<TimelineLayer> layers;   // 0번이 맨 위(화면 앞)
    std::vector<TimelineCue> cues;
    TimelineEnd endAction = TimelineEnd::Hold;

    double length() const;               // 마지막 클립이 끝나는 시각
    bool operator==(const Timeline& o) const;
    bool operator!=(const Timeline& o) const { return !(*this == o); }
};

// ---------------------------------------------------------------- 장면

enum class SceneKind {
    Regions,   // 영역별 플레이리스트/자막
    Timeline   // 큐 타임라인
};

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
    SceneKind kind = SceneKind::Regions;
    std::vector<RegionContent> contents;   // kind == Regions
    Timeline timeline;                     // kind == Timeline

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

// ---------------------------------------------------------------- 인터랙티브

// 트리거(터치/MIDI 등) -> 명령 연결
struct TriggerMapping {
    std::string trigger;   // 예: "touch:버튼1", "midi:note:60", "midi:cc:20"
    std::string command;   // 예: "scene 행사"
};

struct InteractiveConfig {
    bool udpEnabled = false;
    int udpPort = 7000;
    bool tcpEnabled = false;
    int tcpPort = 7001;
    bool oscEnabled = false;
    int oscPort = 8000;
    bool serialEnabled = false;
    std::string serialPort;            // 예: "COM3"
    int serialBaud = 9600;
    bool midiEnabled = false;
    std::string midiDevice;            // MIDI 입력 장치 이름
    bool touchEnabled = false;         // 출력 화면 터치/클릭 -> "touch:<영역 이름>" 트리거
    std::vector<TriggerMapping> mappings;
};

// ---------------------------------------------------------------- 프로젝트

struct Project {
    CanvasConfig canvas;
    std::vector<Region> regions;      // 배열 순서 = 그리는 순서 (뒤쪽 요소가 앞에 보임)
    std::vector<Scene> scenes;        // 최소 1개
    Schedule schedule;
    std::vector<OutputConfig> outputs;
    bool autoStart = false;           // 프로그램 시작 시 출력 켜기 + 재생(또는 스케줄 운영)
    bool hardwareDecoding = true;     // 영상 하드웨어 디코딩 (영역별 설정이 '전체 설정 따름'일 때)
    std::string audioDevice;          // 소리 출력 장치 이름 (빈 문자열 = 기본 장치)
    InteractiveConfig interactive;

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
const char* timelineEndKey(TimelineEnd e);
bool timelineEndFromKey(const std::string& key, TimelineEnd& out);
const char* decodeModeKey(DecodeMode m);
bool decodeModeFromKey(const std::string& key, DecodeMode& out);
// 영역에 실제로 적용할 디코딩 방식 (true = 하드웨어)
bool effectiveHardwareDecoding(const Project& project, const Region& region);
const char* tickerDirectionKey(TickerDirection d);
bool tickerDirectionFromKey(const std::string& key, TickerDirection& out);

// "#AARRGGBB" <-> ARGB
std::string colorToString(std::uint32_t argb);
bool colorFromString(const std::string& text, std::uint32_t& out);

// "HH:MM" <-> 분
std::string minuteToString(int minute);
bool minuteFromString(const std::string& text, int& out);

} // namespace led
