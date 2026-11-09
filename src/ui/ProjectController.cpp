#include "ui/ProjectController.h"

#include "config/ProjectSerializer.h"
#include "display/DisplayInfo.h"

#include <algorithm>

namespace led {

namespace {

std::string defaultDisplayId(const std::vector<DisplayInfo>& displays)
{
    for (const auto& d : displays)
        if (d.primary)
            return d.id;
    return displays.empty() ? std::string() : displays.front().id;
}

} // namespace

ProjectController::ProjectController(QObject* parent)
    : QObject(parent)
{
    newProject();
}

void ProjectController::setDirty(bool dirty)
{
    if (m_dirty == dirty)
        return;
    m_dirty = dirty;
    emit dirtyChanged(dirty);
}

void ProjectController::newProject()
{
    Project p;
    p.canvas = CanvasConfig{1920, 1080};

    OutputConfig out;
    out.id = generateId();
    out.name = "출력 1";
    out.displayId = defaultDisplayId(enumerateDisplays());
    out.canvasRect = RectI{0, 0, 1920, 1080};
    p.outputs.push_back(out);

    Region region;
    region.id = generateId();
    region.name = "영역 1";
    region.rect = RectI{0, 0, 1920, 1080};
    p.regions.push_back(region);

    Scene scene;
    scene.id = generateId();
    scene.name = "장면 1";
    p.scenes.push_back(scene);
    p.schedule.defaultSceneId = scene.id;

    m_project = std::move(p);
    m_filePath.clear();
    m_selectedRegion = QString::fromStdString(region.id);
    m_editingScene = QString::fromStdString(scene.id);
    setDirty(false);
    emit projectReset();
    emit regionSelected(m_selectedRegion);
    emit editingSceneChanged(m_editingScene);
}

bool ProjectController::load(const QString& path, QString* error)
{
    Project p;
    if (!loadProjectFile(path, p, error))
        return false;
    m_project = std::move(p);
    m_filePath = path;
    m_selectedRegion = m_project.regions.empty() ? QString() : QString::fromStdString(m_project.regions.back().id);
    m_editingScene = QString::fromStdString(m_project.scenes.front().id);
    setDirty(false);
    emit projectReset();
    emit regionSelected(m_selectedRegion);
    emit editingSceneChanged(m_editingScene);
    return true;
}

bool ProjectController::save(const QString& path, QString* error)
{
    if (!saveProjectFile(m_project, path, error))
        return false;
    m_filePath = path;
    setDirty(false);
    return true;
}

QString ProjectController::addRegion()
{
    Region r;
    r.id = generateId();
    r.name = "영역 " + std::to_string(m_project.regions.size() + 1);
    r.rect = RectI{0, 0, std::min(960, m_project.canvas.width), std::min(540, m_project.canvas.height)};
    m_project.regions.push_back(r);
    m_selectedRegion = QString::fromStdString(r.id);
    setDirty(true);
    emit regionsChanged();
    emit regionSelected(m_selectedRegion);
    return m_selectedRegion;
}

void ProjectController::removeRegion(const QString& id)
{
    auto& regions = m_project.regions;
    const auto it = std::find_if(regions.begin(), regions.end(),
                                 [&](const Region& r) { return r.id == id.toStdString(); });
    if (it == regions.end())
        return;
    regions.erase(it);
    for (Scene& scene : m_project.scenes) {
        auto& c = scene.contents;
        c.erase(std::remove_if(c.begin(), c.end(),
                               [&](const RegionContent& rc) { return rc.regionId == id.toStdString(); }),
                c.end());
        for (TimelineLayer& layer : scene.timeline.layers)
            if (layer.regionId == id.toStdString())
                layer.regionId.clear(); // 영역이 없어진 레이어는 소리만 재생
    }
    if (m_selectedRegion == id)
        m_selectedRegion = regions.empty() ? QString() : QString::fromStdString(regions.back().id);
    setDirty(true);
    emit regionsChanged();
    emit regionSelected(m_selectedRegion);
}

void ProjectController::updateRegion(const Region& region)
{
    Region* target = m_project.findRegion(region.id);
    if (!target)
        return;
    Region fixed = region;
    fixed.rect.w = std::max(1, fixed.rect.w);
    fixed.rect.h = std::max(1, fixed.rect.h);
    fixed.opacity = std::clamp(fixed.opacity, 0.0, 1.0);
    *target = fixed;
    setDirty(true);
    emit regionsChanged();
}

void ProjectController::moveRegion(const QString& id, int delta)
{
    auto& regions = m_project.regions;
    const auto it = std::find_if(regions.begin(), regions.end(),
                                 [&](const Region& r) { return r.id == id.toStdString(); });
    if (it == regions.end())
        return;
    const int index = static_cast<int>(it - regions.begin());
    const int target = index + delta;
    if (target < 0 || target >= static_cast<int>(regions.size()))
        return;
    std::swap(regions[static_cast<std::size_t>(index)], regions[static_cast<std::size_t>(target)]);
    setDirty(true);
    emit regionsChanged();
}

void ProjectController::selectRegion(const QString& id)
{
    if (m_selectedRegion == id)
        return;
    m_selectedRegion = id;
    emit regionSelected(id);
}

QString ProjectController::addOutput()
{
    const std::vector<DisplayInfo> displays = enumerateDisplays();
    OutputConfig out;
    out.id = generateId();
    out.name = "출력 " + std::to_string(m_project.outputs.size() + 1);

    // 아직 쓰지 않은 디스플레이 우선 선택
    out.displayId = defaultDisplayId(displays);
    for (const auto& d : displays) {
        const bool used = std::any_of(m_project.outputs.begin(), m_project.outputs.end(),
                                      [&](const OutputConfig& o) { return o.displayId == d.id; });
        if (!used) {
            out.displayId = d.id;
            break;
        }
    }

    // 마지막 출력 오른쪽에 이어 붙임
    RectI rect{0, 0, 1920, 1080};
    if (const DisplayInfo* d = findDisplay(displays, out.displayId)) {
        rect.w = d->width;
        rect.h = d->height;
    }
    if (!m_project.outputs.empty()) {
        const RectI& last = m_project.outputs.back().canvasRect;
        rect.x = last.right();
        rect.y = last.y;
    }
    out.canvasRect = rect;
    m_project.outputs.push_back(out);
    setDirty(true);
    emit outputsChanged();
    return QString::fromStdString(out.id);
}

void ProjectController::removeOutput(const QString& id)
{
    auto& outputs = m_project.outputs;
    const auto it = std::find_if(outputs.begin(), outputs.end(),
                                 [&](const OutputConfig& o) { return o.id == id.toStdString(); });
    if (it == outputs.end())
        return;
    outputs.erase(it);
    setDirty(true);
    emit outputsChanged();
}

void ProjectController::updateOutput(const OutputConfig& output)
{
    OutputConfig* target = m_project.findOutput(output.id);
    if (!target)
        return;
    OutputConfig fixed = output;
    fixed.canvasRect.w = std::max(1, fixed.canvasRect.w);
    fixed.canvasRect.h = std::max(1, fixed.canvasRect.h);
    *target = fixed;
    setDirty(true);
    emit outputsChanged();
}

void ProjectController::setCanvasSize(int width, int height)
{
    width = std::clamp(width, 1, kMaxCanvasSize);
    height = std::clamp(height, 1, kMaxCanvasSize);
    if (width == m_project.canvas.width && height == m_project.canvas.height)
        return;
    m_project.canvas.width = width;
    m_project.canvas.height = height;
    setDirty(true);
    emit canvasChanged();
}

// ---------------------------------------------------------------- 장면

const Scene* ProjectController::editingScene() const
{
    return m_project.findScene(m_editingScene.toStdString());
}

void ProjectController::setEditingScene(const QString& id)
{
    if (m_editingScene == id || !m_project.findScene(id.toStdString()))
        return;
    m_editingScene = id;
    emit editingSceneChanged(id);
}

QString ProjectController::addScene(SceneKind kind)
{
    Scene scene;
    scene.id = generateId();
    scene.kind = kind;
    scene.name = (kind == SceneKind::Timeline ? "타임라인 " : "장면 ") + std::to_string(m_project.scenes.size() + 1);
    if (kind == SceneKind::Timeline) {
        TimelineLayer layer;
        layer.id = generateId();
        layer.name = "레이어 1";
        layer.regionId = m_selectedRegion.toStdString();
        if (!m_project.findRegion(layer.regionId) && !m_project.regions.empty())
            layer.regionId = m_project.regions.front().id;
        scene.timeline.layers.push_back(layer);
    }
    m_project.scenes.push_back(scene);
    setDirty(true);
    emit scenesChanged();
    const QString id = QString::fromStdString(scene.id);
    setEditingScene(id);
    return id;
}

QString ProjectController::duplicateScene(const QString& id)
{
    const Scene* source = m_project.findScene(id.toStdString());
    if (!source)
        return {};
    Scene copy = *source;
    copy.id = generateId();
    copy.name = source->name + " (복사)";
    for (RegionContent& c : copy.contents)
        for (PlaylistItem& item : c.items)
            item.id = generateId();
    m_project.scenes.push_back(copy);
    setDirty(true);
    emit scenesChanged();
    const QString newId = QString::fromStdString(copy.id);
    setEditingScene(newId);
    return newId;
}

void ProjectController::renameScene(const QString& id, const QString& name)
{
    Scene* scene = m_project.findScene(id.toStdString());
    const std::string text = name.trimmed().toStdString();
    if (!scene || text.empty() || scene->name == text)
        return;
    scene->name = text;
    setDirty(true);
    emit scenesChanged();
}

bool ProjectController::removeScene(const QString& id)
{
    auto& scenes = m_project.scenes;
    if (scenes.size() <= 1)
        return false;
    const auto it = std::find_if(scenes.begin(), scenes.end(),
                                 [&](const Scene& s) { return s.id == id.toStdString(); });
    if (it == scenes.end())
        return false;
    scenes.erase(it);
    if (m_project.schedule.defaultSceneId == id.toStdString())
        m_project.schedule.defaultSceneId = scenes.front().id;
    setDirty(true);
    emit scenesChanged();
    emit scheduleChanged();
    if (m_editingScene == id) {
        m_editingScene = QString::fromStdString(scenes.front().id);
        emit editingSceneChanged(m_editingScene);
    }
    return true;
}

void ProjectController::moveScene(const QString& id, int delta)
{
    auto& scenes = m_project.scenes;
    const auto it = std::find_if(scenes.begin(), scenes.end(),
                                 [&](const Scene& s) { return s.id == id.toStdString(); });
    if (it == scenes.end())
        return;
    const int index = static_cast<int>(it - scenes.begin());
    const int target = index + delta;
    if (target < 0 || target >= static_cast<int>(scenes.size()))
        return;
    std::swap(scenes[static_cast<std::size_t>(index)], scenes[static_cast<std::size_t>(target)]);
    setDirty(true);
    emit scenesChanged();
}

// ---------------------------------------------------------------- 콘텐츠

const RegionContent* ProjectController::contentFor(const QString& sceneId, const QString& regionId) const
{
    const Scene* scene = m_project.findScene(sceneId.toStdString());
    return scene ? scene->contentFor(regionId.toStdString()) : nullptr;
}

void ProjectController::setContent(const QString& sceneId, const RegionContent& content)
{
    Scene* scene = m_project.findScene(sceneId.toStdString());
    if (!scene || !m_project.findRegion(content.regionId))
        return;
    RegionContent* existing = scene->contentFor(content.regionId);
    if (existing) {
        if (*existing == content)
            return;
        *existing = content;
    } else {
        scene->contents.push_back(content);
    }
    setDirty(true);
    emit contentChanged(sceneId, QString::fromStdString(content.regionId));
}

// ---------------------------------------------------------------- 스케줄 / 운영

void ProjectController::setSchedule(const Schedule& schedule)
{
    m_project.schedule = schedule;
    setDirty(true);
    emit scheduleChanged();
}

void ProjectController::setScheduleEnabled(bool enabled)
{
    if (m_project.schedule.enabled == enabled)
        return;
    m_project.schedule.enabled = enabled;
    setDirty(true);
    emit scheduleChanged();
}

void ProjectController::setAutoStart(bool enabled)
{
    if (m_project.autoStart == enabled)
        return;
    m_project.autoStart = enabled;
    setDirty(true);
    emit scheduleChanged();
}

void ProjectController::setHardwareDecoding(bool enabled)
{
    if (m_project.hardwareDecoding == enabled)
        return;
    m_project.hardwareDecoding = enabled;
    setDirty(true);
    emit playbackSettingsChanged();
}

void ProjectController::setInteractive(const InteractiveConfig& config)
{
    m_project.interactive = config;
    setDirty(true);
    emit interactiveChanged();
}

void ProjectController::setSceneKind(const QString& id, SceneKind kind)
{
    Scene* scene = m_project.findScene(id.toStdString());
    if (!scene || scene->kind == kind)
        return;
    scene->kind = kind;
    if (kind == SceneKind::Timeline && scene->timeline.layers.empty()) {
        TimelineLayer layer;
        layer.id = generateId();
        layer.name = "레이어 1";
        layer.regionId = m_project.regions.empty() ? std::string() : m_project.regions.front().id;
        scene->timeline.layers.push_back(layer);
    }
    setDirty(true);
    emit scenesChanged();
    emit timelineChanged(id);
}

void ProjectController::setTimeline(const QString& sceneId, const Timeline& timeline)
{
    Scene* scene = m_project.findScene(sceneId.toStdString());
    if (!scene || scene->timeline == timeline)
        return;
    scene->timeline = timeline;
    setDirty(true);
    emit timelineChanged(sceneId);
}

void ProjectController::setAudioDevice(const QString& name)
{
    if (m_project.audioDevice == name.toStdString())
        return;
    m_project.audioDevice = name.toStdString();
    setDirty(true);
    emit playbackSettingsChanged();
}

} // namespace led
