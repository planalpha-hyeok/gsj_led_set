#pragma once

// 출력(그래픽카드 출력 = 출력 창) 목록과 속성 편집

#include "display/DisplayInfo.h"

#include <QWidget>

#include <vector>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;

namespace led {

class ProjectController;

class OutputPanel : public QWidget {
    Q_OBJECT

public:
    explicit OutputPanel(ProjectController* controller, QWidget* parent = nullptr);

public slots:
    void refreshDisplays();

private slots:
    void refreshList();
    void loadSelected();
    void commit();
    void applyDisplaySize();

private:
    void fillDisplayCombo(const std::string& currentId);

    ProjectController* m_controller;
    std::vector<DisplayInfo> m_displays;
    QString m_selectedId;

    QListWidget* m_list;
    QPushButton* m_addButton;
    QPushButton* m_removeButton;
    QPushButton* m_refreshButton;

    QWidget* m_form;
    QLineEdit* m_name;
    QComboBox* m_display;
    QLabel* m_displayInfo;
    QSpinBox* m_x;
    QSpinBox* m_y;
    QSpinBox* m_w;
    QSpinBox* m_h;
    QPushButton* m_displaySizeButton;
    QSpinBox* m_offsetX;
    QSpinBox* m_offsetY;
    QCheckBox* m_enabled;

    bool m_loading = false;
};

} // namespace led
