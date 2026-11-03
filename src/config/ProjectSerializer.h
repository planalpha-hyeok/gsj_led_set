#pragma once

// 프로젝트 파일(.ledproj, JSON) 저장/불러오기
// 버전 1 파일(1단계)은 불러올 때 자동으로 버전 2 구조로 변환

#include "core/Project.h"

#include <QByteArray>
#include <QString>

namespace led {

constexpr int kProjectFileVersion = 2;
constexpr int kMaxCanvasSize = 65536;

QByteArray projectToJson(const Project& project);
bool projectFromJson(const QByteArray& json, Project& out, QString* error);

bool saveProjectFile(const Project& project, const QString& path, QString* error);
bool loadProjectFile(const QString& path, Project& out, QString* error);

// 장면이 하나도 없으면 기본 장면 생성, 없는 영역을 가리키는 콘텐츠 제거
void normalizeProject(Project& project);

} // namespace led
