#pragma once

// 흐르는 자막 편집 (문구/파일, 글꼴, 색, 외곽선, 그림자, 속도, 방향)

#include "core/Project.h"

#include <QTimer>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QFontComboBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;

namespace led {

class ColorButton;

class TickerEditor : public QWidget {
    Q_OBJECT

public:
    explicit TickerEditor(QWidget* parent = nullptr);

    void setContent(const RegionContent& content);

signals:
    void edited(const RegionContent& content);

private slots:
    void commit();
    void browseFile();
    void clearFile();

private:
    void updateEnabled();

    RegionContent m_content;
    QPlainTextEdit* m_lines;
    QTimer m_linesTimer;
    QLineEdit* m_file;
    QPushButton* m_browseButton;
    QPushButton* m_clearButton;
    QFontComboBox* m_font;
    QSpinBox* m_size;
    QCheckBox* m_bold;
    ColorButton* m_textColor;
    ColorButton* m_backgroundColor;
    QSpinBox* m_outlineWidth;
    ColorButton* m_outlineColor;
    QCheckBox* m_shadow;
    QSpinBox* m_shadowOffset;
    ColorButton* m_shadowColor;
    QSpinBox* m_speed;
    QComboBox* m_direction;
    QSpinBox* m_gap;
    bool m_loading = false;
};

} // namespace led
