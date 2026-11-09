#pragma once

// 플레이리스트 <-> 타임라인 레이어 변환 (표준 C++만 사용)
//  - 영역의 '플레이리스트 레이어' = 타임라인에서 그 영역의 첫 번째 레이어
//  - 플레이리스트 항목은 레이어에 빈틈없이 차례로 배치 (페이드 전환 = 앞 클립과 겹침)
//  - 목록 반복 = 레이어 반복, 반복을 끄면 마지막 화면 유지 (이미 반복이 꺼져 있던 레이어는 기존 설정 유지)

#include "core/Project.h"

#include <functional>
#include <string>

namespace led {

// 자동 길이 클립의 실제 길이(초). 0 이하 = 알 수 없음 (파일 정보 조회는 media 모듈에서 제공)
using DurationResolver = std::function<double(const TimelineClip&)>;

constexpr double kDefaultLiveDuration = 3600.0;
constexpr double kDefaultTickerDuration = 60.0;

// 자동 길이 클립의 길이를 채우고, 클립을 현재 순서대로 이어 붙임
// resolve가 비어 있으면 길이는 그대로 두고 (0 이하만 기본값) 다시 배치만 함
void packLayer(TimelineLayer& layer, const DurationResolver& resolve);

// 반복 레이어 한 바퀴 길이 (첫 클립의 페이드만큼 마지막 클립과 겹쳐 이어짐)
double layerCycleLength(const TimelineLayer& layer);

// 반복 레이어에 0초에서 시작하는 클립 1개만 있으면 끊김 없이 계속 재생
//   (자동 길이 파일 = 파일 반복, 라이브/자막 = 계속). 길이를 직접 정한 파일은 그 길이로 반복
bool isContinuousLayer(const TimelineLayer& layer);

// 영역의 플레이리스트 레이어 번호 (없으면 -1)
int playlistLayerIndex(const Timeline& timeline, const std::string& regionId);

// 플레이리스트 화면으로 그대로 편집할 수 있는 레이어인지
//   (빈틈없이 차례로 배치, 자막은 단독 클립 1개)
//   타임라인에서 자유롭게 배치한 레이어는 false -> 타임라인에서만 편집
bool isPlaylistCompatible(const TimelineLayer& layer);

// 레이어 -> 플레이리스트 화면 내용 (layer가 nullptr이면 빈 플레이리스트)
RegionContent layerToContent(const TimelineLayer* layer, const std::string& regionId);

// 플레이리스트 화면 내용 -> 레이어 (같은 id의 클립은 다른 속성(불투명도/소리/자르기 등)을 유지)
void contentToLayer(const RegionContent& content, TimelineLayer& layer, const DurationResolver& resolve);

} // namespace led
