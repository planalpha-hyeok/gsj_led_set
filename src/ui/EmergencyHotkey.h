#pragma once

// 비상 단축키 Ctrl+Alt+F12 (Windows 전역 단축키)
// 출력 화면이 편집 화면을 가려 클릭할 수 없을 때도 동작함

#include <QAbstractNativeEventFilter>
#include <QObject>

class QWidget;

namespace led {

class EmergencyHotkey : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT

public:
    explicit EmergencyHotkey(QWidget* window, QObject* parent = nullptr);
    ~EmergencyHotkey() override;

    bool isRegistered() const { return m_registered; }
    static QString keyText();

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

signals:
    void triggered();

private:
    void* m_hwnd = nullptr;
    bool m_registered = false;
};

} // namespace led
