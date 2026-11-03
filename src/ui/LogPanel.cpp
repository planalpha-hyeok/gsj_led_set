#include "ui/LogPanel.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace led {

LogPanel::LogPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    m_text = new QPlainTextEdit(this);
    m_text->setReadOnly(true);
    m_text->setMaximumBlockCount(3000);
    m_text->setObjectName("logView");
    layout->addWidget(m_text, 1);

    auto* row = new QHBoxLayout;
    row->addStretch(1);
    auto* clear = new QPushButton(QStringLiteral("기록 지우기"), this);
    row->addWidget(clear);
    layout->addLayout(row);
    connect(clear, &QPushButton::clicked, m_text, &QPlainTextEdit::clear);
}

void LogPanel::append(int level, const QString& message)
{
    const QString time = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    const char* color = level >= 2 ? "#ff6b6b" : level == 1 ? "#f2c14e" : "#b8bcc4";
    const char* tag = level >= 2 ? "오류" : level == 1 ? "경고" : "정보";
    m_text->appendHtml(QStringLiteral("<span style=\"color:#7d828c\">%1</span> "
                                      "<span style=\"color:%2\">[%3] %4</span>")
                           .arg(time, QString::fromLatin1(color), QString::fromUtf8(tag), message.toHtmlEscaped()));
}

} // namespace led
