#pragma once

// 스케줄 편집: 운영 설정(사용, 일정 외 동작, 자동 시작) + 일정 목록(요일/날짜, 시각, 장면)

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDateEdit;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QRadioButton;
class QTimeEdit;

namespace led {

class ProjectController;

class SchedulePanel : public QWidget {
    Q_OBJECT

public:
    explicit SchedulePanel(ProjectController* controller, QWidget* parent = nullptr);

public slots:
    void setStatusText(const QString& text);

private slots:
    void refresh();
    void loadEntry();
    void commitSettings();
    void commitEntry();
    void addEntry();
    void removeEntry();
    void moveEntry(int delta);

private:
    int currentIndex() const;
    void fillSceneCombo(QComboBox* combo, const std::string& selectedId);

    ProjectController* m_controller;
    QString m_selectedEntry;

    QCheckBox* m_enabled;
    QComboBox* m_outside;
    QComboBox* m_defaultScene;
    QCheckBox* m_autoStart;
    QLabel* m_status;

    QListWidget* m_list;
    QPushButton* m_addButton;
    QPushButton* m_removeButton;
    QPushButton* m_upButton;
    QPushButton* m_downButton;

    QWidget* m_entryForm;
    QLineEdit* m_name;
    QCheckBox* m_entryEnabled;
    QComboBox* m_scene;
    QRadioButton* m_weekdayMode;
    QRadioButton* m_dateMode;
    QCheckBox* m_days[7];
    QDateEdit* m_date;
    QTimeEdit* m_start;
    QTimeEdit* m_end;
    QLabel* m_entryHint;

    bool m_loading = false;
};

} // namespace led
