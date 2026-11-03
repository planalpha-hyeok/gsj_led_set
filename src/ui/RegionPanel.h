#pragma once

// 영역 목록, 선택된 영역의 위치/표시 방식, 콘텐츠(편집 중인 장면 기준) 편집

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;

namespace led {

class ContentEditor;
class ProjectController;

class RegionPanel : public QWidget {
    Q_OBJECT

public:
    explicit RegionPanel(ProjectController* controller, QWidget* parent = nullptr);

private slots:
    void refreshList();
    void loadSelected();
    void onListSelectionChanged();
    void commit();

private:
    QString currentId() const;

    ProjectController* m_controller;
    QListWidget* m_list;
    QPushButton* m_addButton;
    QPushButton* m_removeButton;
    QPushButton* m_upButton;
    QPushButton* m_downButton;

    QWidget* m_form;
    QLineEdit* m_name;
    QSpinBox* m_x;
    QSpinBox* m_y;
    QSpinBox* m_w;
    QSpinBox* m_h;
    QComboBox* m_fit;
    QSpinBox* m_opacity;
    QCheckBox* m_visible;
    ContentEditor* m_content;

    bool m_loading = false;
};

} // namespace led
