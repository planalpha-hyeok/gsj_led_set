#pragma once

// 프로젝트 파일(.ledproj, JSON) 저장/불러오기

#include "core/Project.h"

#include <QByteArray>
#include <QString>

namespace led {

constexpr int kProjectFileVersion = 1;
constexpr int kMaxCanvasSize = 65536;

QByteArray projectToJson(const Project& project);
bool projectFromJson(const QByteArray& json, Project& out, QString* error);

bool saveProjectFile(const Project& project, const QString& path, QString* error);
bool loadProjectFile(const QString& path, Project& out, QString* error);

} // namespace led
