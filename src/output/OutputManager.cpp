#include "output/OutputManager.h"

#include "display/DisplayInfo.h"
#include "media/QImageFrame.h"
#include "testpattern/TestPattern.h"

namespace led {

OutputManager::OutputManager(SceneProvider* scene, QObject* parent)
    : QObject(parent)
    , m_scene(scene)
{
}

OutputManager::~OutputManager()
{
    stop();
}

void OutputManager::start(const Project& project, QStringList& errors)
{
    stop();
    const std::vector<DisplayInfo> displays = enumerateDisplays();

    for (int i = 0; i < static_cast<int>(project.outputs.size()); ++i) {
        const OutputConfig& oc = project.outputs[static_cast<std::size_t>(i)];
        const QString name = QString::fromStdString(oc.name);
        if (!oc.enabled)
            continue;
        if (oc.canvasRect.isEmpty()) {
            errors << QStringLiteral("%1: 출력 크기가 0").arg(name);
            continue;
        }
        const DisplayInfo* display = findDisplay(displays, oc.displayId);
        if (!display) {
            errors << QStringLiteral("%1: 디스플레이 '%2'를 찾을 수 없음 (연결 상태 확인)")
                          .arg(name, QString::fromStdString(oc.displayId));
            continue;
        }

        OutputSpec spec;
        spec.name = oc.name;
        spec.windowX = display->x + oc.offsetX;
        spec.windowY = display->y + oc.offsetY;
        spec.canvasRect = oc.canvasRect;
        // 주 모니터(편집 화면이 있는 화면)에서는 항상 위 표시를 하지 않음
        // -> Alt+Tab, 작업 관리자로 편집 화면에 돌아갈 수 있음
        spec.topmost = !display->primary;

        auto window = createOutputWindow();
        std::string error;
        if (!window->start(spec, m_scene, error)) {
            errors << QStringLiteral("%1: %2").arg(name, QString::fromStdString(error));
            continue;
        }
        m_windows.push_back(Running{oc.id, i, std::move(window)});
    }

    if (m_testPattern)
        applyOverlays(project);
}

void OutputManager::stop()
{
    for (Running& r : m_windows)
        r.window->stop();
    m_windows.clear();
}

void OutputManager::setTestPattern(bool enabled, const Project& project)
{
    m_testPattern = enabled;
    applyOverlays(project);
}

void OutputManager::applyOverlays(const Project& project)
{
    for (Running& r : m_windows) {
        if (!m_testPattern) {
            r.window->setOverlay(nullptr);
            continue;
        }
        const OutputConfig* oc = project.findOutput(r.outputId);
        if (!oc)
            continue;
        r.window->setOverlay(frameFromQImage(makeTestPattern(project, *oc, r.index)));
    }
}

} // namespace led
