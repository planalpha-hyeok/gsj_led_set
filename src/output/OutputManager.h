#pragma once

// 프로젝트의 출력 설정에 따라 출력 창들을 만들고 관리

#include "core/Project.h"
#include "output/OutputWindow.h"

#include <QObject>
#include <QStringList>

#include <memory>
#include <vector>

namespace led {

class OutputManager : public QObject {
    Q_OBJECT

public:
    explicit OutputManager(SceneProvider* scene, QObject* parent = nullptr);
    ~OutputManager() override;

    // 사용 설정된 출력을 모두 시작. errors: 시작하지 못한 출력과 사유
    void start(const Project& project, QStringList& errors);
    void stop();
    bool isRunning() const { return !m_windows.empty(); }
    int runningCount() const { return static_cast<int>(m_windows.size()); }

    void setTestPattern(bool enabled, const Project& project);
    bool testPatternEnabled() const { return m_testPattern; }

private:
    struct Running {
        std::string outputId;
        int index = 0;
        std::unique_ptr<OutputWindow> window;
    };

    void applyOverlays(const Project& project);

    SceneProvider* m_scene;
    std::vector<Running> m_windows;
    bool m_testPattern = false;
};

} // namespace led
