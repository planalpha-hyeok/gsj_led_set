#pragma once

// Windows 이외 환경용 출력 창 (개발/검사용, QPainter 기반)
// 실제 현장 출력은 Win32OutputWindow(Direct3D 11) 사용

#include "output/OutputWindow.h"

#include <QRasterWindow>
#include <QTimer>

#include <memory>
#include <mutex>

namespace led {

class QtOutputSurface : public QRasterWindow {
public:
    QtOutputSurface(const OutputSpec& spec, SceneProvider* scene);
    void setOverlay(std::shared_ptr<const Frame> overlay);
    void setClickHandler(OutputWindow::ClickHandler handler) { m_clickHandler = std::move(handler); }

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    OutputSpec m_spec;
    SceneProvider* m_scene;
    std::shared_ptr<const Frame> m_overlay;
    QTimer m_timer;
    OutputWindow::ClickHandler m_clickHandler;
};

class QtOutputWindow final : public OutputWindow {
public:
    ~QtOutputWindow() override;
    bool start(const OutputSpec& spec, SceneProvider* scene, std::string& error) override;
    void stop() override;
    void setOverlay(std::shared_ptr<const Frame> overlay) override;
    std::string adapterName() const override { return "QPainter (개발용)"; }
    void setClickHandler(ClickHandler handler) override { m_clickHandler = std::move(handler); }

private:
    ClickHandler m_clickHandler;
    std::unique_ptr<QtOutputSurface> m_surface;
};

} // namespace led
