#include "app/LogBridge.h"

#include "core/Log.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMetaObject>
#include <QStandardPaths>
#include <QTextStream>

#include <memory>

namespace led {

LogBridge::LogBridge(QObject* parent)
    : QObject(parent)
{
}

LogBridge* LogBridge::instance()
{
    static LogBridge bridge;
    return &bridge;
}

QString LogBridge::install()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
                        QStringLiteral("/logs");
    QDir().mkpath(dir);
    const QString path = dir + QStringLiteral("/player.log");

    // 1MB를 넘으면 이전 로그를 .old로 보관
    QFile existing(path);
    if (existing.exists() && existing.size() > 1024 * 1024) {
        QFile::remove(path + QStringLiteral(".old"));
        QFile::rename(path, path + QStringLiteral(".old"));
    }

    auto file = std::make_shared<QFile>(path);
    const bool fileOk = file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);

    setLogSink([this, file, fileOk](LogLevel level, const std::string& message) {
        const QString text = QString::fromStdString(message);
        const int lv = level == LogLevel::Error ? 2 : level == LogLevel::Warning ? 1 : 0;
        if (fileOk) {
            const char* tag = lv == 2 ? "ERROR" : lv == 1 ? "WARN " : "INFO ";
            const QString line = QStringLiteral("%1 [%2] %3\n")
                                     .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
                                          QString::fromLatin1(tag), text);
            file->write(line.toUtf8());
            file->flush();
        }
        // 어느 스레드에서 호출되든 GUI 스레드에서 신호 발생
        QMetaObject::invokeMethod(
            this, [this, lv, text] { emit messageLogged(lv, text); }, Qt::QueuedConnection);
    });
    return fileOk ? path : QString();
}

void LogBridge::uninstall()
{
    setLogSink(nullptr);
}

} // namespace led
