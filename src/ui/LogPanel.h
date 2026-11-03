#pragma once

// 프로그램 동작/오류 기록 표시

#include <QWidget>

class QPlainTextEdit;

namespace led {

class LogPanel : public QWidget {
    Q_OBJECT

public:
    explicit LogPanel(QWidget* parent = nullptr);

public slots:
    // level: 0 정보, 1 경고, 2 오류
    void append(int level, const QString& message);

private:
    QPlainTextEdit* m_text;
};

} // namespace led
