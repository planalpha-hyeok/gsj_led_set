#pragma once

// 영역 목록과 선택된 영역의 속성(좌표, 콘텐츠, 맞춤 방식 등) 편집

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;

namespace led {

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
    void browseSource();
    void clearSource();

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
    QLineEdit* m_source;
    QPushButton* m_browseButton;
    QPushButton* m_clearButton;
    QComboBox* m_fit;
    QCheckBox* m_loop;
    QSpinBox* m_opacity;
    QCheckBox* m_visible;

    bool m_loading = false;
};

} // namespace led
