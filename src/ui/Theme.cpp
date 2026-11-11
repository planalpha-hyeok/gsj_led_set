#include "ui/Theme.h"

#include "ui/Palette.h"

#include <QApplication>
#include <QFont>
#include <QPalette>
#include <QStyleFactory>

namespace led {

namespace {

// 스타일시트의 @이름을 색으로 바꿈
QString fillColors(QString sheet)
{
    using namespace palette;
    const struct {
        const char* key;
        QColor color;
    } colors[] = {{"@bg", kBackground},   {"@panel", kPanel},           {"@raised", kRaised},
                  {"@hover", kHover},     {"@border2", kBorderStrong},  {"@border", kBorder},
                  {"@text2", kTextSecondary}, {"@textdim", kTextDim},   {"@textoff", kTextDisabled},
                  {"@text", kText},       {"@live", kLive},             {"@selectbg", kSelectBg},
                  {"@select", kSelect},   {"@dangerbg", kDangerBg},     {"@danger", kDanger}};
    // 긴 이름부터 바꿔야 @border2 가 @border 로 잘리지 않음 (위 목록 순서)
    for (const auto& c : colors)
        sheet.replace(QLatin1String(c.key), css(c.color));
    sheet.replace(QLatin1String("@mono"), monoFontFamily());
    return sheet;
}

} // namespace

void applyTheme(QApplication& app)
{
    using namespace palette;
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

#ifdef Q_OS_WIN
    QFont font(QStringLiteral("Malgun Gothic"), 9);
    app.setFont(font);
#endif

    QPalette p;
    p.setColor(QPalette::Window, kPanel);
    p.setColor(QPalette::WindowText, kText);
    p.setColor(QPalette::Base, kBackground);
    p.setColor(QPalette::AlternateBase, kRaised);
    p.setColor(QPalette::ToolTipBase, kRaised);
    p.setColor(QPalette::ToolTipText, kText);
    p.setColor(QPalette::PlaceholderText, kTextDisabled);
    p.setColor(QPalette::Text, kText);
    p.setColor(QPalette::Button, kRaised);
    p.setColor(QPalette::ButtonText, kText);
    p.setColor(QPalette::BrightText, Qt::white);
    p.setColor(QPalette::Highlight, kSelect);
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::Link, kSelect);
    p.setColor(QPalette::Mid, kBorder);
    p.setColor(QPalette::Dark, kBackground);
    p.setColor(QPalette::Disabled, QPalette::Text, kTextDisabled);
    p.setColor(QPalette::Disabled, QPalette::WindowText, kTextDisabled);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, kTextDisabled);
    app.setPalette(p);

    app.setStyleSheet(fillColors(QStringLiteral(R"(
QMainWindow { background: @bg; }
QMainWindow::separator { background: @border; width: 1px; height: 1px; }
QMenuBar { background: @panel; color: @text2; border-bottom: 1px solid @border; }
QMenuBar::item { padding: 6px 10px; background: transparent; }
QMenuBar::item:selected { background: @hover; color: @text; }
QMenu { background: @raised; border: 1px solid @border2; padding: 4px; }
QMenu::item { padding: 6px 24px 6px 12px; border-radius: 3px; }
QMenu::item:selected { background: @selectbg; color: @text; }
QMenu::separator { height: 1px; background: @border; margin: 4px 6px; }

QToolBar { background: @panel; border: none; border-bottom: 1px solid @border; spacing: 4px; padding: 6px 8px; }
QToolBar::separator { background: @border; width: 1px; margin: 4px 6px; }
QToolButton { padding: 6px 10px; border-radius: 4px; color: @text2; border: 1px solid transparent; }
QToolButton:hover { background: @hover; color: @text; }
QToolButton:checked { background: @selectbg; color: @text; border-color: @select; }
QToolButton:pressed { background: @border2; }
QLabel#clockLabel { font-family: @mono; font-size: 13px; padding: 4px 12px; background: @bg;
                    border: 1px solid @border; border-radius: 6px; color: @text; }

QDockWidget { color: @text; titlebar-close-icon: none; }
QDockWidget::title { background: @panel; padding: 7px 10px; border-bottom: 1px solid @border; text-align: left; }

QTabWidget::pane { border: none; border-top: 1px solid @border; top: -1px; }
QTabBar::tab { background: transparent; color: @textdim; padding: 9px 14px; border: none; border-bottom: 2px solid transparent; }
QTabBar::tab:hover { color: @text; }
QTabBar::tab:selected { color: @text; border-bottom: 2px solid @live; }

QGroupBox { border: 1px solid @border; border-radius: 6px; margin-top: 16px; padding: 12px 10px 10px 10px; }
QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; color: @textdim; }

QListWidget, QTreeWidget, QTableWidget, QTableView { background: @bg; border: 1px solid @border; border-radius: 4px; outline: none; }
QListWidget::item { padding: 6px 8px; border-left: 2px solid transparent; }
QListWidget::item:hover { background: @raised; }
QListWidget::item:selected { background: @selectbg; color: @text; border-left: 2px solid @live; }
QHeaderView::section { background: @raised; color: @textdim; border: none; border-right: 1px solid @border; padding: 5px 8px; }

QPushButton { padding: 6px 12px; border: 1px solid @border2; border-radius: 4px; background: @raised; color: @text2; }
QPushButton:hover { background: @hover; color: @text; }
QPushButton:pressed { background: @selectbg; }
QPushButton:checked { background: @selectbg; border-color: @select; color: @text; }
QPushButton:disabled { color: @textoff; border-color: @border; background: @panel; }
QPushButton:default { border-color: @select; }

QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox, QPlainTextEdit, QTextEdit, QTimeEdit, QDateEdit {
    padding: 4px 6px; border: 1px solid @border2; border-radius: 4px; background: @bg; color: @text;
    selection-background-color: @select; }
QSpinBox, QDoubleSpinBox, QTimeEdit { font-family: @mono; }
QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus, QPlainTextEdit:focus, QTextEdit:focus { border-color: @select; }
QLineEdit:disabled, QSpinBox:disabled, QDoubleSpinBox:disabled, QComboBox:disabled { color: @textoff; border-color: @border; }
QComboBox QAbstractItemView { background: @raised; border: 1px solid @border2; selection-background-color: @selectbg; }

QCheckBox, QRadioButton { spacing: 6px; color: @text2; }
QCheckBox::indicator, QRadioButton::indicator { width: 14px; height: 14px; }

QScrollBar:vertical { background: @panel; width: 10px; margin: 0; }
QScrollBar:horizontal { background: @panel; height: 10px; margin: 0; }
QScrollBar::handle { background: @border2; border-radius: 4px; min-height: 24px; min-width: 24px; }
QScrollBar::handle:hover { background: @textoff; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: none; }

QSlider::groove:horizontal { height: 4px; background: @border2; border-radius: 2px; }
QSlider::handle:horizontal { width: 12px; margin: -5px 0; border-radius: 6px; background: @text2; }
QSplitter::handle { background: @border; }
QToolTip { background: @raised; color: @text; border: 1px solid @border2; padding: 5px; }

QLabel#sectionLabel { color: @textdim; font-weight: bold; }
QLabel#hintLabel { color: @textdim; }
QStatusBar { background: @panel; border-top: 1px solid @border; color: @textdim; }
QStatusBar QLabel { color: @text2; }
QPlainTextEdit#logView { font-family: @mono, "Malgun Gothic"; }
)")));
}

} // namespace led
