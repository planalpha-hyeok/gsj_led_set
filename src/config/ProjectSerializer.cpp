#include "config/ProjectSerializer.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <algorithm>

namespace led {

namespace {

QString qs(const std::string& s) { return QString::fromStdString(s); }
std::string ss(const QString& s) { return s.toStdString(); }

QJsonObject rectToJson(const RectI& r)
{
    QJsonObject o;
    o["x"] = r.x;
    o["y"] = r.y;
    o["width"] = r.w;
    o["height"] = r.h;
    return o;
}

RectI rectFromJson(const QJsonObject& o)
{
    RectI r;
    r.x = o.value("x").toInt(0);
    r.y = o.value("y").toInt(0);
    r.w = o.value("width").toInt(0);
    r.h = o.value("height").toInt(0);
    return r;
}

void setError(QString* error, const QString& msg)
{
    if (error)
        *error = msg;
}

} // namespace

QByteArray projectToJson(const Project& project)
{
    QJsonObject root;
    root["version"] = kProjectFileVersion;

    QJsonObject canvas;
    canvas["width"] = project.canvas.width;
    canvas["height"] = project.canvas.height;
    root["canvas"] = canvas;

    QJsonArray regions;
    for (const auto& r : project.regions) {
        QJsonObject o = rectToJson(r.rect);
        o["id"] = qs(r.id);
        o["name"] = qs(r.name);
        o["source"] = qs(r.source);
        o["fit"] = QString::fromLatin1(fitModeKey(r.fit));
        o["loop"] = r.loop;
        o["opacity"] = r.opacity;
        o["visible"] = r.visible;
        regions.append(o);
    }
    root["regions"] = regions;

    QJsonArray outputs;
    for (const auto& out : project.outputs) {
        QJsonObject o;
        o["id"] = qs(out.id);
        o["name"] = qs(out.name);
        o["display"] = qs(out.displayId);
        o["canvasRect"] = rectToJson(out.canvasRect);
        o["offsetX"] = out.offsetX;
        o["offsetY"] = out.offsetY;
        o["enabled"] = out.enabled;
        outputs.append(o);
    }
    root["outputs"] = outputs;

    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

bool projectFromJson(const QByteArray& json, Project& out, QString* error)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (doc.isNull() || !doc.isObject()) {
        setError(error, QStringLiteral("JSON 형식 오류: %1 (위치 %2)")
                            .arg(parseError.errorString())
                            .arg(parseError.offset));
        return false;
    }

    const QJsonObject root = doc.object();
    const int version = root.value("version").toInt(0);
    if (version < 1 || version > kProjectFileVersion) {
        setError(error, QStringLiteral("지원하지 않는 프로젝트 파일 버전: %1").arg(version));
        return false;
    }

    Project p;

    const QJsonObject canvas = root.value("canvas").toObject();
    p.canvas.width = canvas.value("width").toInt(0);
    p.canvas.height = canvas.value("height").toInt(0);
    if (p.canvas.width < 1 || p.canvas.height < 1 || p.canvas.width > kMaxCanvasSize ||
        p.canvas.height > kMaxCanvasSize) {
        setError(error, QStringLiteral("캔버스 크기가 잘못됨: %1 x %2").arg(p.canvas.width).arg(p.canvas.height));
        return false;
    }

    for (const QJsonValue& v : root.value("regions").toArray()) {
        const QJsonObject o = v.toObject();
        Region r;
        r.id = ss(o.value("id").toString());
        if (r.id.empty())
            r.id = generateId();
        r.name = ss(o.value("name").toString());
        r.rect = rectFromJson(o);
        r.source = ss(o.value("source").toString());
        if (!fitModeFromKey(ss(o.value("fit").toString("stretch")), r.fit))
            r.fit = FitMode::Stretch;
        r.loop = o.value("loop").toBool(true);
        r.opacity = std::clamp(o.value("opacity").toDouble(1.0), 0.0, 1.0);
        r.visible = o.value("visible").toBool(true);
        if (r.rect.w < 1 || r.rect.h < 1) {
            setError(error, QStringLiteral("영역 '%1'의 크기가 잘못됨").arg(qs(r.name)));
            return false;
        }
        p.regions.push_back(std::move(r));
    }

    for (const QJsonValue& v : root.value("outputs").toArray()) {
        const QJsonObject o = v.toObject();
        OutputConfig oc;
        oc.id = ss(o.value("id").toString());
        if (oc.id.empty())
            oc.id = generateId();
        oc.name = ss(o.value("name").toString());
        oc.displayId = ss(o.value("display").toString());
        oc.canvasRect = rectFromJson(o.value("canvasRect").toObject());
        oc.offsetX = o.value("offsetX").toInt(0);
        oc.offsetY = o.value("offsetY").toInt(0);
        oc.enabled = o.value("enabled").toBool(true);
        if (oc.canvasRect.w < 1 || oc.canvasRect.h < 1) {
            setError(error, QStringLiteral("출력 '%1'의 크기가 잘못됨").arg(qs(oc.name)));
            return false;
        }
        p.outputs.push_back(std::move(oc));
    }

    out = std::move(p);
    return true;
}

bool saveProjectFile(const Project& project, const QString& path, QString* error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        setError(error, QStringLiteral("파일을 쓸 수 없음: %1 (%2)").arg(path, file.errorString()));
        return false;
    }
    const QByteArray data = projectToJson(project);
    if (file.write(data) != data.size()) {
        setError(error, QStringLiteral("파일 쓰기 실패: %1").arg(file.errorString()));
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        setError(error, QStringLiteral("파일 저장 실패: %1").arg(file.errorString()));
        return false;
    }
    return true;
}

bool loadProjectFile(const QString& path, Project& out, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, QStringLiteral("파일을 열 수 없음: %1 (%2)").arg(path, file.errorString()));
        return false;
    }
    return projectFromJson(file.readAll(), out, error);
}

} // namespace led
