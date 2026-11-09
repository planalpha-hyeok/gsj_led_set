#pragma once

// 프로젝트 파일(.ledproj, JSON) 저장/불러오기
// 버전 1/2 파일은 불러올 때 자동으로 버전 3 구조(모든 장면 = 타임라인)로 변환
//   변환된 플레이리스트 레이어는 needsPacking = true (파일 길이를 알아야 배치 가능)

#include "core/Project.h"

#include <QByteArray>
#include <QString>

namespace led {

constexpr int kProjectFileVersion = 3;
constexpr int kMaxCanvasSize = 65536;

QByteArray projectToJson(const Project& project);
bool projectFromJson(const QByteArray& json, Project& out, QString* error);

bool saveProjectFile(const Project& project, const QString& path, QString* error);
bool loadProjectFile(const QString& path, Project& out, QString* error);

// 장면이 하나도 없으면 기본 장면 생성, 없는 영역을 가리키는 레이어는 소리 전용으로
void normalizeProject(Project& project);

} // namespace led
