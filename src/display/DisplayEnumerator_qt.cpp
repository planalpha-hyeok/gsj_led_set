// Windows 이외 환경(개발/검사용): Qt 화면 정보로 디스플레이 조회

#include "display/DisplayInfo.h"

#include <QGuiApplication>
#include <QScreen>

#include <algorithm>
#include <cmath>

namespace led {

std::vector<DisplayInfo> enumerateDisplays()
{
    std::vector<DisplayInfo> list;
    const QScreen* primary = QGuiApplication::primaryScreen();
    for (const QScreen* screen : QGuiApplication::screens()) {
        const QRect g = screen->geometry();
        const double dpr = screen->devicePixelRatio();
        DisplayInfo d;
        d.id = screen->name().toStdString();
        d.name = (screen->manufacturer() + QLatin1Char(' ') + screen->model()).trimmed().toStdString();
        d.x = static_cast<int>(std::lround(g.x() * dpr));
        d.y = static_cast<int>(std::lround(g.y() * dpr));
        d.width = static_cast<int>(std::lround(g.width() * dpr));
        d.height = static_cast<int>(std::lround(g.height() * dpr));
        d.refreshHz = static_cast<int>(std::lround(screen->refreshRate()));
        d.primary = screen == primary;
        list.push_back(std::move(d));
    }
    std::sort(list.begin(), list.end(), [](const DisplayInfo& a, const DisplayInfo& b) {
        return a.x != b.x ? a.x < b.x : a.y < b.y;
    });
    return list;
}

} // namespace led
