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

    m_project = std::move(p);
    m_filePath.clear();
    m_selectedRegion = QString::fromStdString(region.id);
    setDirty(false);
    emit projectReset();
    emit regionSelected(m_selectedRegion);
}

bool ProjectController::load(const QString& path, QString* error)
{
    Project p;
    if (!loadProjectFile(path, p, error))
        return false;
    m_project = std::move(p);
    m_filePath = path;
    m_selectedRegion = m_project.regions.empty() ? QString() : QString::fromStdString(m_project.regions.back().id);
    setDirty(false);
    emit projectReset();
    emit regionSelected(m_selectedRegion);
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

} // namespace led
