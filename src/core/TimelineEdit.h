#pragma once

// 타임라인 편집 계산 (표준 C++만 사용): 클립 찾기/이동/자르기/자석/삭제

#include "core/Project.h"

#include <set>
#include <string>
#include <vector>

namespace led {

constexpr double kMinClipDuration = 0.1; // 초

struct ClipLocation {
    int layer = -1;
    int index = -1;
    bool valid() const { return layer >= 0 && index >= 0; }
};

ClipLocation findClip(const Timeline& tl, const std::string& clipId);
TimelineClip* clipById(Timeline& tl, const std::string& clipId);
const TimelineClip* clipById(const Timeline& tl, const std::string& clipId);

// 자석 기준점: 0초, 재생 위치, 큐, (제외 목록에 없는) 클립의 시작/끝
std::vector<double> timelineSnapPoints(const Timeline& tl, const std::set<std::string>& excludeClipIds,
                                       double playhead);

// value에서 threshold 이내의 가장 가까운 기준점. 없으면 value 그대로 (snapped=false)
double snapTime(double value, const std::vector<double>& points, double threshold, bool* snapped = nullptr);

// 여러 클립 이동 (시각 + 레이어). 잠긴 레이어/범위 밖이면 아무것도 바꾸지 않고 false
// 이동 후 시작 시각이 0보다 작아지지 않도록 deltaTime을 보정
bool moveClips(Timeline& tl, const std::vector<std::string>& clipIds, double deltaTime, int deltaLayer);

// 앞쪽 자르기: 클립 시작을 newStart로 (끝은 그대로, 자르기 지점도 같이 이동). 자르기 지점은 0 미만 불가
bool trimClipStart(Timeline& tl, const std::string& clipId, double newStart);
// 뒤쪽 자르기: 클립 끝을 newEnd로. maxDuration > 0 이면 길이를 그 이하로 제한
bool trimClipEnd(Timeline& tl, const std::string& clipId, double newEnd, double maxDuration);

// 클립 삭제 (잠긴 레이어의 클립은 남김). 삭제한 개수 반환
int deleteClips(Timeline& tl, const std::vector<std::string>& clipIds);

// 클립 추가 (레이어가 잠겨 있으면 false)
bool addClip(Timeline& tl, int layer, const TimelineClip& clip);

} // namespace led
