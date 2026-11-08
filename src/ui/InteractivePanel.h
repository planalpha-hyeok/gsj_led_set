#pragma once

// 인터랙티브 설정: 외부 입력(UDP/TCP/OSC/시리얼/MIDI/터치), 트리거 연결표, 명령 시험

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

namespace led {

class ProjectController;

class InteractivePanel : public QWidget {
    Q_OBJECT

public:
    explicit InteractivePanel(ProjectController* controller, QWidget* parent = nullptr);

public slots:
    void setStatusText(const QString& text);
    void setLastTrigger(const QString& trigger, const QString& source);
    void setCommandResult(const QString& text);

signals:
    // '실행' 버튼으로 명령 시험
    void commandRequested(const QString& command);

private slots:
    void load();
    void commit();
    void refreshPorts();
    void addMapping();
    void removeMapping();

private:
    ProjectController* m_controller;
    bool m_loading = false;
    bool m_committing = false;

    QCheckBox* m_udpEnabled;
    QSpinBox* m_udpPort;
    QCheckBox* m_tcpEnabled;
    QSpinBox* m_tcpPort;
    QCheckBox* m_oscEnabled;
    QSpinBox* m_oscPort;
    QCheckBox* m_serialEnabled;
    QComboBox* m_serialPort;
    QComboBox* m_serialBaud;
    QCheckBox* m_midiEnabled;
    QComboBox* m_midiDevice;
    QCheckBox* m_touchEnabled;
    QLabel* m_status;

    QTableWidget* m_mappings;
    QLabel* m_lastTrigger;
    QString m_lastTriggerName;
    QPushButton* m_useLastTrigger;

    QLineEdit* m_testCommand;
    QLabel* m_testResult;
};

} // namespace led
