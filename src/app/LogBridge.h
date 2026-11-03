#pragma once

// core 로그(모든 스레드) -> 화면 로그 창 + 로그 파일 연결

#include <QObject>
#include <QString>

namespace led {

class LogBridge : public QObject {
    Q_OBJECT

public:
    static LogBridge* instance();

    // 로그 파일 경로 반환 (실패 시 빈 문자열)
    QString install();
    void uninstall();

signals:
    void messageLogged(int level, const QString& message);

private:
    explicit LogBridge(QObject* parent = nullptr);
};

} // namespace led
