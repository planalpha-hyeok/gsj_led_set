// GSJ LED 플레이어 진입점

#include "app/LogBridge.h"
#include "core/Log.h"
#include "ui/LogPanel.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QImageReader>

int main(int argc, char* argv[])
{
    // 화면 배율(125%, 150% 등)이 있어도 좌표 계산이 정확하도록 배율을 반올림하지 않음
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("GSJ LED Player"));
    QApplication::setOrganizationName(QStringLiteral("GSJ"));
    QApplication::setApplicationVersion(QStringLiteral(LED_PLAYER_VERSION));

    // 대형 캔버스용 고해상도 이미지 허용 (기본 256MB 제한 -> 2GB)
    QImageReader::setAllocationLimit(2048);

    led::applyTheme(app);

    led::LogBridge* bridge = led::LogBridge::instance();
    const QString logPath = bridge->install();

    int result = 0;
    {
        led::MainWindow window;
        QObject::connect(bridge, &led::LogBridge::messageLogged, window.logPanel(), &led::LogPanel::append);
        led::logInfo("GSJ LED 플레이어 " LED_PLAYER_VERSION " 시작");
        if (!logPath.isEmpty())
            led::logInfo("로그 파일: " + logPath.toStdString());

        window.show();
        const QStringList args = QApplication::arguments();
        if (args.size() > 1)
            window.openProjectFile(args.at(1));

        result = app.exec();
    }
    bridge->uninstall();
    return result;
}
