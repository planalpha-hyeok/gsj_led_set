#include "ui/EmergencyHotkey.h"

#include "core/Log.h"

#include <QCoreApplication>
#include <QWidget>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace led {

namespace {
constexpr int kHotkeyId = 0x4C45; // 임의의 고유 번호
}

QString EmergencyHotkey::keyText()
{
    return QStringLiteral("Ctrl+Alt+F12");
}

EmergencyHotkey::EmergencyHotkey(QWidget* window, QObject* parent)
    : QObject(parent)
{
#ifdef Q_OS_WIN
    m_hwnd = reinterpret_cast<void*>(window->winId());
    m_registered = RegisterHotKey(static_cast<HWND>(m_hwnd), kHotkeyId, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT,
                                  VK_F12) != 0;
    if (m_registered) {
        QCoreApplication::instance()->installNativeEventFilter(this);
        logInfo("비상 단축키 등록: Ctrl+Alt+F12 (모든 출력 끄기)");
    } else {
        logWarning("비상 단축키 Ctrl+Alt+F12 등록 실패 (다른 프로그램이 사용 중일 수 있음)");
    }
#else
    Q_UNUSED(window);
#endif
}

EmergencyHotkey::~EmergencyHotkey()
{
#ifdef Q_OS_WIN
    if (m_registered) {
        QCoreApplication::instance()->removeNativeEventFilter(this);
        UnregisterHotKey(static_cast<HWND>(m_hwnd), kHotkeyId);
    }
#endif
}

bool EmergencyHotkey::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result)
{
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG") {
        const MSG* msg = static_cast<const MSG*>(message);
        if (msg->message == WM_HOTKEY && static_cast<int>(msg->wParam) == kHotkeyId) {
            emit triggered();
            if (result)
                *result = 0;
            return true;
        }
    }
#else
    Q_UNUSED(eventType);
    Q_UNUSED(message);
    Q_UNUSED(result);
#endif
    return false;
}

} // namespace led
