#include "ui/TickerEditor.h"

#include "ui/ColorButton.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>

namespace led {

namespace {

QSpinBox* makeSpin(int min, int max, const QString& suffix, QWidget* parent)
{
    auto* box = new QSpinBox(parent);
    box->setRange(min, max);
    box->setSuffix(suffix);
    box->setKeyboardTracking(false);
    return box;
}

} // namespace

TickerEditor::TickerEditor(QWidget* parent)
    : QWidget(parent)
{
    auto* form = new QFormLayout(this);
    form->setContentsMargins(0, 0, 0, 0);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_lines = new QPlainTextEdit(this);
    m_lines->setPlaceholderText(QStringLiteral("한 줄에 문구 하나씩 입력 (순서대로 흐름)"));
    m_lines->setMinimumHeight(80);
    m_lines->setMaximumHeight(140);
    form->addRow(QStringLiteral("문구"), m_lines);

    auto* fileRow = new QHBoxLayout;
    m_file = new QLineEdit(this);
    m_file->setReadOnly(true);
    m_file->setPlaceholderText(QStringLiteral("사용 안 함"));
    m_browseButton = new QPushButton(QStringLiteral("찾기..."), this);
    m_clearButton = new QPushButton(QStringLiteral("비우기"), this);
    fileRow->addWidget(m_file, 1);
    fileRow->addWidget(m_browseButton);
    fileRow->addWidget(m_clearButton);
    form->addRow(QStringLiteral("자막 파일"), fileRow);
    auto* fileHint = new QLabel(QStringLiteral("파일 지정 시 위 문구 대신 파일 내용(.txt, 줄마다 문구 1개)을 사용하며,\n"
                                               "재생 중 파일을 저장하면 1초 안에 새 내용으로 다시 흐릅니다."),
                                this);
    fileHint->setObjectName("hintLabel");
    form->addRow(QString(), fileHint);

    auto* fontRow = new QHBoxLayout;
    m_font = new QFontComboBox(this);
    m_size = makeSpin(4, 4000, QStringLiteral(" px"), this);
    m_bold = new QCheckBox(QStringLiteral("굵게"), this);
    fontRow->addWidget(m_font, 1);
    fontRow->addWidget(m_size);
    fontRow->addWidget(m_bold);
    form->addRow(QStringLiteral("글꼴"), fontRow);

    auto* colorRow = new QHBoxLayout;
    m_textColor = new ColorButton(this);
    m_backgroundColor = new ColorButton(this);
    colorRow->addWidget(new QLabel(QStringLiteral("글자"), this));
    colorRow->addWidget(m_textColor);
    colorRow->addWidget(new QLabel(QStringLiteral("배경"), this));
    colorRow->addWidget(m_backgroundColor);
    colorRow->addStretch(1);
    form->addRow(QStringLiteral("색"), colorRow);

    auto* outlineRow = new QHBoxLayout;
    m_outlineWidth = makeSpin(0, 200, QStringLiteral(" px"), this);
    m_outlineWidth->setSpecialValueText(QStringLiteral("없음"));
    m_outlineColor = new ColorButton(this);
    outlineRow->addWidget(m_outlineWidth);
    outlineRow->addWidget(m_outlineColor);
    outlineRow->addStretch(1);
    form->addRow(QStringLiteral("외곽선"), outlineRow);

    auto* shadowRow = new QHBoxLayout;
    m_shadow = new QCheckBox(QStringLiteral("사용"), this);
    m_shadowOffset = makeSpin(0, 200, QStringLiteral(" px"), this);
    m_shadowColor = new ColorButton(this);
    shadowRow->addWidget(m_shadow);
    shadowRow->addWidget(m_shadowOffset);
    shadowRow->addWidget(m_shadowColor);
    shadowRow->addStretch(1);
    form->addRow(QStringLiteral("그림자"), shadowRow);

    auto* motionRow = new QHBoxLayout;
    m_direction = new QComboBox(this);
    m_direction->addItem(QStringLiteral("← 왼쪽으로"), int(TickerDirection::Left));
    m_direction->addItem(QStringLiteral("→ 오른쪽으로"), int(TickerDirection::Right));
    m_direction->addItem(QStringLiteral("↑ 위로"), int(TickerDirection::Up));
    m_direction->addItem(QStringLiteral("↓ 아래로"), int(TickerDirection::Down));
    m_speed = makeSpin(1, 10000, QStringLiteral(" px/초"), this);
    motionRow->addWidget(m_direction, 1);
    motionRow->addWidget(m_speed);
    form->addRow(QStringLiteral("흐름"), motionRow);

    m_gap = makeSpin(0, 100000, QStringLiteral(" px"), this);
    form->addRow(QStringLiteral("문구 간격"), m_gap);

    m_linesTimer.setSingleShot(true);
    m_linesTimer.setInterval(600); // 입력이 멈춘 뒤 반영
    connect(&m_linesTimer, &QTimer::timeout, this, &TickerEditor::commit);
    connect(m_lines, &QPlainTextEdit::textChanged, this, [this] {
        if (!m_loading)
            m_linesTimer.start();
    });

    connect(m_browseButton, &QPushButton::clicked, this, &TickerEditor::browseFile);
    connect(m_clearButton, &QPushButton::clicked, this, &TickerEditor::clearFile);
    connect(m_font, &QFontComboBox::currentFontChanged, this, &TickerEditor::commit);
    for (QSpinBox* box : {m_size, m_outlineWidth, m_shadowOffset, m_speed, m_gap})
        connect(box, QOverload<int>::of(&QSpinBox::valueChanged), this, &TickerEditor::commit);
    connect(m_bold, &QCheckBox::toggled, this, &TickerEditor::commit);
    connect(m_shadow, &QCheckBox::toggled, this, &TickerEditor::commit);
    connect(m_direction, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TickerEditor::commit);
    for (ColorButton* b : {m_textColor, m_backgroundColor, m_outlineColor, m_shadowColor})
        connect(b, &ColorButton::colorChanged, this, &TickerEditor::commit);
}

void TickerEditor::setContent(const RegionContent& content)
{
    m_content = content;
    const TickerConfig& t = content.ticker;
    m_loading = true;

    QStringList lines;
    for (const auto& l : t.lines)
        lines << QString::fromStdString(l);
    const QString text = lines.join(QLatin1Char('\n'));
    if (m_lines->toPlainText() != text && !m_linesTimer.isActive())
        m_lines->setPlainText(text);

    m_file->setText(t.textFile.empty() ? QString() : QFileInfo(QString::fromStdString(t.textFile)).fileName());
    m_file->setToolTip(QString::fromStdString(t.textFile));
    m_font->setCurrentFont(QFont(QString::fromStdString(t.fontFamily)));
    m_size->setValue(t.fontSize);
    m_bold->setChecked(t.bold);
    m_textColor->setColor(t.textColor);
    m_backgroundColor->setColor(t.backgroundColor);
    m_outlineWidth->setValue(t.outlineWidth);
    m_outlineColor->setColor(t.outlineColor);
    m_shadow->setChecked(t.shadow);
    m_shadowOffset->setValue(t.shadowOffset);
    m_shadowColor->setColor(t.shadowColor);
    m_speed->setValue(static_cast<int>(t.speed));
    m_direction->setCurrentIndex(std::max(0, m_direction->findData(int(t.direction))));
    m_gap->setValue(t.gap);
    m_loading = false;
    updateEnabled();
}

void TickerEditor::updateEnabled()
{
    const bool usesFile = !m_content.ticker.textFile.empty();
    m_lines->setEnabled(!usesFile);
    m_clearButton->setEnabled(usesFile);
    m_outlineColor->setEnabled(m_outlineWidth->value() > 0);
    m_shadowOffset->setEnabled(m_shadow->isChecked());
    m_shadowColor->setEnabled(m_shadow->isChecked());
}

void TickerEditor::commit()
{
    if (m_loading)
        return;
    m_linesTimer.stop();
    TickerConfig& t = m_content.ticker;
    const TickerConfig before = t;

    t.lines.clear();
    for (const QString& line : m_lines->toPlainText().split(QLatin1Char('\n'))) {
        const QString trimmed = line.trimmed();
        if (!trimmed.isEmpty())
            t.lines.push_back(trimmed.toStdString());
    }
    t.fontFamily = m_font->currentFont().family().toStdString();
    t.fontSize = m_size->value();
    t.bold = m_bold->isChecked();
    t.textColor = m_textColor->color();
    t.backgroundColor = m_backgroundColor->color();
    t.outlineWidth = m_outlineWidth->value();
    t.outlineColor = m_outlineColor->color();
    t.shadow = m_shadow->isChecked();
    t.shadowOffset = m_shadowOffset->value();
    t.shadowColor = m_shadowColor->color();
    t.speed = m_speed->value();
    t.direction = static_cast<TickerDirection>(m_direction->currentData().toInt());
    t.gap = m_gap->value();
    updateEnabled();
    if (t != before)
        emit edited(m_content);
}

void TickerEditor::browseFile()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("자막 파일 선택"), QString(),
                                                      QStringLiteral("텍스트 파일 (*.txt);;모든 파일 (*.*)"));
    if (path.isEmpty())
        return;
    m_content.ticker.textFile = path.toStdString();
    setContent(m_content);
    emit edited(m_content);
}

void TickerEditor::clearFile()
{
    if (m_content.ticker.textFile.empty())
        return;
    m_content.ticker.textFile.clear();
    setContent(m_content);
    emit edited(m_content);
}

} // namespace led
