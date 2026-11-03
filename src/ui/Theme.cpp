#include "ui/Theme.h"

#include <QApplication>
#include <QFont>
#include <QPalette>
#include <QStyleFactory>

namespace led {

void applyTheme(QApplication& app)
{
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

#ifdef Q_OS_WIN
    QFont font(QStringLiteral("Malgun Gothic"), 9);
    app.setFont(font);
#endif

    const QColor window(43, 45, 48);
    const QColor base(30, 31, 34);
    const QColor alternate(37, 39, 42);
    const QColor text(223, 225, 229);
    const QColor disabledText(120, 123, 130);
    const QColor button(55, 58, 62);
    const QColor highlight(53, 116, 240);

    QPalette p;
    p.setColor(QPalette::Window, window);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, alternate);
    p.setColor(QPalette::ToolTipBase, base);
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::PlaceholderText, disabledText);
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, button);
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::BrightText, Qt::white);
    p.setColor(QPalette::Highlight, highlight);
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::Link, highlight);
    p.setColor(QPalette::Disabled, QPalette::Text, disabledText);
    p.setColor(QPalette::Disabled, QPalette::WindowText, disabledText);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, disabledText);
    app.setPalette(p);

    app.setStyleSheet(QStringLiteral(R"(
QToolBar { background: #232427; border: none; border-bottom: 1px solid #3a3d41; spacing: 4px; padding: 4px; }
QToolButton { padding: 5px 10px; border-radius: 4px; color: #dfe1e5; }
QToolButton:hover { background: #3a3d42; }
QToolButton:checked { background: #2f5fbf; color: white; }
QDockWidget::title { background: #232427; padding: 6px 8px; border-bottom: 1px solid #3a3d41; }
QTabWidget::pane { border: 1px solid #3a3d41; top: -1px; }
QTabBar::tab { background: #2b2d30; padding: 6px 14px; border: 1px solid #3a3d41; border-bottom: none; }
QTabBar::tab:selected { background: #35373b; color: white; }
QGroupBox { border: 1px solid #3a3d41; border-radius: 6px; margin-top: 14px; padding: 10px 8px 8px 8px; }
QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; color: #9aa0aa; }
QListWidget { border: 1px solid #3a3d41; border-radius: 4px; }
QListWidget::item { padding: 4px 6px; }
QPushButton { padding: 5px 12px; border: 1px solid #4a4d52; border-radius: 4px; background: #37393e; }
QPushButton:hover { background: #42454a; }
QPushButton:pressed { background: #2f5fbf; }
QPushButton:disabled { color: #6f737a; border-color: #3a3d41; }
QLineEdit, QSpinBox, QComboBox { padding: 3px 5px; border: 1px solid #4a4d52; border-radius: 4px; background: #1e1f22; }
QLineEdit:focus, QSpinBox:focus, QComboBox:focus { border-color: #3574f0; }
QLabel#sectionLabel { color: #9aa0aa; font-weight: bold; }
QLabel#hintLabel { color: #8b9099; }
QStatusBar { background: #232427; border-top: 1px solid #3a3d41; }
QPlainTextEdit#logView { font-family: Consolas, "Malgun Gothic", monospace; }
)"));
}

} // namespace led
