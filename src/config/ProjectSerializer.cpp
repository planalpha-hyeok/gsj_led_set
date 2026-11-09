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

void setError(QString* error, const QString& msg)
{
    if (error)
        *error = msg;
}

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
    return RectI{o.value("x").toInt(0), o.value("y").toInt(0), o.value("width").toInt(0), o.value("height").toInt(0)};
}

std::uint32_t colorValue(const QJsonObject& o, const char* key, std::uint32_t fallback)
{
    std::uint32_t c = fallback;
    if (!colorFromString(ss(o.value(key).toString()), c))
        return fallback;
    return c;
}

// ---------------------------------------------------------------- 저장

QJsonObject tickerToJson(const TickerConfig& t)
{
    QJsonObject o;
    QJsonArray lines;
    for (const auto& l : t.lines)
        lines.append(qs(l));
    o["lines"] = lines;
    o["textFile"] = qs(t.textFile);
    o["fontFamily"] = qs(t.fontFamily);
    o["fontSize"] = t.fontSize;
    o["bold"] = t.bold;
    o["textColor"] = qs(colorToString(t.textColor));
    o["backgroundColor"] = qs(colorToString(t.backgroundColor));
    o["outlineWidth"] = t.outlineWidth;
    o["outlineColor"] = qs(colorToString(t.outlineColor));
    o["shadow"] = t.shadow;
    o["shadowOffset"] = t.shadowOffset;
    o["shadowColor"] = qs(colorToString(t.shadowColor));
    o["speed"] = t.speed;
    o["direction"] = QString::fromLatin1(tickerDirectionKey(t.direction));
    o["gap"] = t.gap;
    return o;
}

QJsonObject contentToJson(const RegionContent& c)
{
    QJsonObject o;
    o["regionId"] = qs(c.regionId);
    o["kind"] = c.kind == ContentKind::Ticker ? QStringLiteral("ticker") : QStringLiteral("playlist");
    o["loop"] = c.loopPlaylist;
    QJsonArray items;
    for (const auto& it : c.items) {
        QJsonObject i;
        i["id"] = qs(it.id);
        i["type"] = it.type == ItemType::Live ? QStringLiteral("live") : QStringLiteral("file");
        i["source"] = qs(it.source);
        if (it.type == ItemType::Live) {
            i["liveSize"] = qs(it.liveSize);
            i["liveFps"] = it.liveFps;
        }
        i["duration"] = it.duration;
        i["transition"] = QString::fromLatin1(transitionKey(it.transition));
        i["transitionDuration"] = it.transitionDuration;
        items.append(i);
    }
    o["items"] = items;
    o["ticker"] = tickerToJson(c.ticker);
    return o;
}

QJsonObject timelineToJson(const Timeline& t)
{
    QJsonObject o;
    o["endAction"] = QString::fromLatin1(timelineEndKey(t.endAction));
    QJsonArray layers;
    for (const auto& l : t.layers) {
        QJsonObject lo;
        lo["id"] = qs(l.id);
        lo["name"] = qs(l.name);
        lo["regionId"] = qs(l.regionId);
        lo["visible"] = l.visible;
        lo["muted"] = l.muted;
        lo["locked"] = l.locked;
        QJsonArray clips;
        for (const auto& c : l.clips) {
            QJsonObject co;
            co["id"] = qs(c.id);
            co["source"] = qs(c.source);
            co["start"] = c.start;
            co["duration"] = c.duration;
            co["inPoint"] = c.inPoint;
            co["fadeIn"] = c.fadeIn;
            co["fadeOut"] = c.fadeOut;
            co["opacity"] = c.opacity;
            co["volume"] = c.volume;
            co["loop"] = c.loop;
            clips.append(co);
        }
        lo["clips"] = clips;
        layers.append(lo);
    }
    o["layers"] = layers;
    QJsonArray cues;
    for (const auto& c : t.cues) {
        QJsonObject co;
        co["id"] = qs(c.id);
        co["name"] = qs(c.name);
        co["time"] = c.time;
        cues.append(co);
    }
    o["cues"] = cues;
    return o;
}

Timeline timelineFromJson(const QJsonObject& o)
{
    Timeline t;
    TimelineEnd end;
    if (timelineEndFromKey(ss(o.value("endAction").toString()), end))
        t.endAction = end;
    for (const QJsonValue& lv : o.value("layers").toArray()) {
        const QJsonObject lo = lv.toObject();
        TimelineLayer l;
        l.id = ss(lo.value("id").toString());
        if (l.id.empty())
            l.id = generateId();
        l.name = ss(lo.value("name").toString());
        l.regionId = ss(lo.value("regionId").toString());
        l.visible = lo.value("visible").toBool(true);
        l.muted = lo.value("muted").toBool(false);
        l.locked = lo.value("locked").toBool(false);
        for (const QJsonValue& cv : lo.value("clips").toArray()) {
            const QJsonObject co = cv.toObject();
            TimelineClip c;
            c.id = ss(co.value("id").toString());
            if (c.id.empty())
                c.id = generateId();
            c.source = ss(co.value("source").toString());
            c.start = std::max(0.0, co.value("start").toDouble(0.0));
            c.duration = std::max(0.04, co.value("duration").toDouble(5.0));
            c.inPoint = std::max(0.0, co.value("inPoint").toDouble(0.0));
            c.fadeIn = std::clamp(co.value("fadeIn").toDouble(0.0), 0.0, 3600.0);
            c.fadeOut = std::clamp(co.value("fadeOut").toDouble(0.0), 0.0, 3600.0);
            c.opacity = std::clamp(co.value("opacity").toDouble(1.0), 0.0, 1.0);
            c.volume = std::clamp(co.value("volume").toDouble(1.0), 0.0, 1.0);
            c.loop = co.value("loop").toBool(false);
            l.clips.push_back(std::move(c));
        }
        t.layers.push_back(std::move(l));
    }
    for (const QJsonValue& cv : o.value("cues").toArray()) {
        const QJsonObject co = cv.toObject();
        TimelineCue c;
        c.id = ss(co.value("id").toString());
        if (c.id.empty())
            c.id = generateId();
        c.name = ss(co.value("name").toString());
        c.time = std::max(0.0, co.value("time").toDouble(0.0));
        t.cues.push_back(std::move(c));
    }
    return t;
}

QJsonObject interactiveToJson(const InteractiveConfig& c)
{
    QJsonObject o;
    o["udpEnabled"] = c.udpEnabled;
    o["udpPort"] = c.udpPort;
    o["tcpEnabled"] = c.tcpEnabled;
    o["tcpPort"] = c.tcpPort;
    o["oscEnabled"] = c.oscEnabled;
    o["oscPort"] = c.oscPort;
    o["serialEnabled"] = c.serialEnabled;
    o["serialPort"] = qs(c.serialPort);
    o["serialBaud"] = c.serialBaud;
    o["midiEnabled"] = c.midiEnabled;
    o["midiDevice"] = qs(c.midiDevice);
    o["touchEnabled"] = c.touchEnabled;
    QJsonArray mappings;
    for (const auto& m : c.mappings) {
        QJsonObject mo;
        mo["trigger"] = qs(m.trigger);
        mo["command"] = qs(m.command);
        mappings.append(mo);
    }
    o["mappings"] = mappings;
    return o;
}

// ---------------------------------------------------------------- 불러오기

int portValue(const QJsonObject& o, const char* key, int fallback)
{
    const int v = o.value(key).toInt(fallback);
    return v >= 1 && v <= 65535 ? v : fallback;
}

InteractiveConfig interactiveFromJson(const QJsonObject& o)
{
    InteractiveConfig c;
    c.udpEnabled = o.value("udpEnabled").toBool(false);
    c.udpPort = portValue(o, "udpPort", c.udpPort);
    c.tcpEnabled = o.value("tcpEnabled").toBool(false);
    c.tcpPort = portValue(o, "tcpPort", c.tcpPort);
    c.oscEnabled = o.value("oscEnabled").toBool(false);
    c.oscPort = portValue(o, "oscPort", c.oscPort);
    c.serialEnabled = o.value("serialEnabled").toBool(false);
    c.serialPort = ss(o.value("serialPort").toString());
    c.serialBaud = std::clamp(o.value("serialBaud").toInt(c.serialBaud), 300, 4000000);
    c.midiEnabled = o.value("midiEnabled").toBool(false);
    c.midiDevice = ss(o.value("midiDevice").toString());
    c.touchEnabled = o.value("touchEnabled").toBool(false);
    for (const QJsonValue& v : o.value("mappings").toArray()) {
        const QJsonObject mo = v.toObject();
        TriggerMapping m;
        m.trigger = ss(mo.value("trigger").toString().trimmed());
        m.command = ss(mo.value("command").toString().trimmed());
        if (!m.trigger.empty())
            c.mappings.push_back(std::move(m));
    }
    return c;
}

TickerConfig tickerFromJson(const QJsonObject& o)
{
    TickerConfig t;
    t.lines.clear();
    for (const QJsonValue& v : o.value("lines").toArray())
        t.lines.push_back(ss(v.toString()));
    t.textFile = ss(o.value("textFile").toString());
    t.fontFamily = ss(o.value("fontFamily").toString(qs(t.fontFamily)));
    t.fontSize = std::clamp(o.value("fontSize").toInt(t.fontSize), 4, 4000);
    t.bold = o.value("bold").toBool(t.bold);
    t.textColor = colorValue(o, "textColor", t.textColor);
    t.backgroundColor = colorValue(o, "backgroundColor", t.backgroundColor);
    t.outlineWidth = std::clamp(o.value("outlineWidth").toInt(t.outlineWidth), 0, 200);
    t.outlineColor = colorValue(o, "outlineColor", t.outlineColor);
    t.shadow = o.value("shadow").toBool(t.shadow);
    t.shadowOffset = std::clamp(o.value("shadowOffset").toInt(t.shadowOffset), 0, 200);
    t.shadowColor = colorValue(o, "shadowColor", t.shadowColor);
    t.speed = std::clamp(o.value("speed").toDouble(t.speed), 1.0, 10000.0);
    TickerDirection dir;
    if (tickerDirectionFromKey(ss(o.value("direction").toString()), dir))
        t.direction = dir;
    t.gap = std::clamp(o.value("gap").toInt(t.gap), 0, 100000);
    return t;
}

RegionContent contentFromJson(const QJsonObject& o)
{
    RegionContent c;
    c.regionId = ss(o.value("regionId").toString());
    c.kind = o.value("kind").toString() == QLatin1String("ticker") ? ContentKind::Ticker : ContentKind::Playlist;
    c.loopPlaylist = o.value("loop").toBool(true);
    for (const QJsonValue& v : o.value("items").toArray()) {
        const QJsonObject i = v.toObject();
        PlaylistItem item;
        item.id = ss(i.value("id").toString());
        if (item.id.empty())
            item.id = generateId();
        item.type = i.value("type").toString() == QLatin1String("live") ? ItemType::Live : ItemType::File;
        item.source = ss(i.value("source").toString());
        item.liveSize = ss(i.value("liveSize").toString());
        item.liveFps = std::clamp(i.value("liveFps").toDouble(0.0), 0.0, 1000.0);
        item.duration = std::max(0.0, i.value("duration").toDouble(0.0));
        TransitionType t;
        if (transitionFromKey(ss(i.value("transition").toString()), t))
            item.transition = t;
        item.transitionDuration = std::clamp(i.value("transitionDuration").toDouble(1.0), 0.0, 30.0);
        c.items.push_back(std::move(item));
    }
    c.ticker = tickerFromJson(o.value("ticker").toObject());
    return c;
}

bool readRegions(const QJsonObject& root, Project& p, int version, QString* error)
{
    Scene migrated; // 버전 1의 영역별 콘텐츠를 담을 장면
    migrated.id = generateId();
    migrated.name = "기본 장면";

    for (const QJsonValue& v : root.value("regions").toArray()) {
        const QJsonObject o = v.toObject();
        Region r;
        r.id = ss(o.value("id").toString());
        if (r.id.empty())
            r.id = generateId();
        r.name = ss(o.value("name").toString());
        r.rect = rectFromJson(o);
        if (!fitModeFromKey(ss(o.value("fit").toString("stretch")), r.fit))
            r.fit = FitMode::Stretch;
        r.opacity = std::clamp(o.value("opacity").toDouble(1.0), 0.0, 1.0);
        r.visible = o.value("visible").toBool(true);
        if (!decodeModeFromKey(ss(o.value("decode").toString("default")), r.decode))
            r.decode = DecodeMode::Default;
        if (r.rect.w < 1 || r.rect.h < 1) {
            setError(error, QStringLiteral("영역 '%1'의 크기가 잘못됨").arg(qs(r.name)));
            return false;
        }

        if (version == 1) {
            const std::string source = ss(o.value("source").toString());
            if (!source.empty()) {
                RegionContent c;
                c.regionId = r.id;
                c.loopPlaylist = o.value("loop").toBool(true);
                PlaylistItem item;
                item.id = generateId();
                item.source = source;
                c.items.push_back(item);
                migrated.contents.push_back(std::move(c));
            }
        }
        p.regions.push_back(std::move(r));
    }

    if (version == 1)
        p.scenes.push_back(std::move(migrated));
    return true;
}

} // namespace

void normalizeProject(Project& p)
{
    if (p.scenes.empty()) {
        Scene s;
        s.id = generateId();
        s.name = "기본 장면";
        p.scenes.push_back(std::move(s));
    }
    for (Scene& s : p.scenes) {
        if (s.id.empty())
            s.id = generateId();
        auto& c = s.contents;
        c.erase(std::remove_if(c.begin(), c.end(),
                               [&](const RegionContent& rc) { return !p.findRegion(rc.regionId); }),
                c.end());
        for (TimelineLayer& layer : s.timeline.layers)
            if (!layer.regionId.empty() && !p.findRegion(layer.regionId))
                layer.regionId.clear(); // 삭제된 영역: 소리만 재생
    }
    for (ScheduleEntry& e : p.schedule.entries)
        if (e.id.empty())
            e.id = generateId();
}

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
        o["fit"] = QString::fromLatin1(fitModeKey(r.fit));
        o["opacity"] = r.opacity;
        o["visible"] = r.visible;
        o["decode"] = QString::fromLatin1(decodeModeKey(r.decode));
        regions.append(o);
    }
    root["regions"] = regions;

    QJsonArray scenes;
    for (const auto& s : project.scenes) {
        QJsonObject o;
        o["id"] = qs(s.id);
        o["name"] = qs(s.name);
        o["kind"] = s.kind == SceneKind::Timeline ? QStringLiteral("timeline") : QStringLiteral("regions");
        if (s.kind == SceneKind::Timeline)
            o["timeline"] = timelineToJson(s.timeline);
        QJsonArray contents;
        for (const auto& c : s.contents)
            contents.append(contentToJson(c));
        o["contents"] = contents;
        scenes.append(o);
    }
    root["scenes"] = scenes;

    QJsonObject schedule;
    schedule["enabled"] = project.schedule.enabled;
    schedule["outside"] = project.schedule.outside == OutsideBehavior::DefaultScene ? QStringLiteral("defaultScene")
                                                                                     : QStringLiteral("black");
    schedule["defaultSceneId"] = qs(project.schedule.defaultSceneId);
    QJsonArray entries;
    for (const auto& e : project.schedule.entries) {
        QJsonObject o;
        o["id"] = qs(e.id);
        o["name"] = qs(e.name);
        o["sceneId"] = qs(e.sceneId);
        o["enabled"] = e.enabled;
        o["weekdays"] = static_cast<int>(e.weekdays);
        o["date"] = qs(e.date);
        o["start"] = qs(minuteToString(e.startMinute));
        o["end"] = qs(minuteToString(e.endMinute));
        entries.append(o);
    }
    schedule["entries"] = entries;
    root["schedule"] = schedule;

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
    root["autoStart"] = project.autoStart;
    root["hardwareDecoding"] = project.hardwareDecoding;
    root["audioDevice"] = qs(project.audioDevice);
    root["interactive"] = interactiveToJson(project.interactive);

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

    if (!readRegions(root, p, version, error))
        return false;

    if (version >= 2) {
        for (const QJsonValue& v : root.value("scenes").toArray()) {
            const QJsonObject o = v.toObject();
            Scene s;
            s.id = ss(o.value("id").toString());
            s.name = ss(o.value("name").toString());
            s.kind = o.value("kind").toString() == QLatin1String("timeline") ? SceneKind::Timeline : SceneKind::Regions;
            if (s.kind == SceneKind::Timeline)
                s.timeline = timelineFromJson(o.value("timeline").toObject());
            for (const QJsonValue& c : o.value("contents").toArray())
                s.contents.push_back(contentFromJson(c.toObject()));
            p.scenes.push_back(std::move(s));
        }

        const QJsonObject schedule = root.value("schedule").toObject();
        p.schedule.enabled = schedule.value("enabled").toBool(false);
        p.schedule.outside = schedule.value("outside").toString() == QLatin1String("defaultScene")
                                 ? OutsideBehavior::DefaultScene
                                 : OutsideBehavior::Black;
        p.schedule.defaultSceneId = ss(schedule.value("defaultSceneId").toString());
        for (const QJsonValue& v : schedule.value("entries").toArray()) {
            const QJsonObject o = v.toObject();
            ScheduleEntry e;
            e.id = ss(o.value("id").toString());
            e.name = ss(o.value("name").toString());
            e.sceneId = ss(o.value("sceneId").toString());
            e.enabled = o.value("enabled").toBool(true);
            e.weekdays = static_cast<std::uint8_t>(o.value("weekdays").toInt(0x7F) & 0x7F);
            e.date = ss(o.value("date").toString());
            if (!minuteFromString(ss(o.value("start").toString()), e.startMinute) ||
                !minuteFromString(ss(o.value("end").toString()), e.endMinute)) {
                setError(error, QStringLiteral("일정 '%1'의 시각 형식이 잘못됨 (HH:MM)").arg(qs(e.name)));
                return false;
            }
            p.schedule.entries.push_back(std::move(e));
        }
        p.autoStart = root.value("autoStart").toBool(false);
        p.hardwareDecoding = root.value("hardwareDecoding").toBool(true);
        p.audioDevice = ss(root.value("audioDevice").toString());
        p.interactive = interactiveFromJson(root.value("interactive").toObject());
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

    normalizeProject(p);
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
