#pragma once

// MIDI 입력 (Windows winmm). 다른 운영체제에서는 장치 없음
// 받은 신호를 트리거 이름으로 변환:
//   노트 켜기 -> "midi:note:<번호>", 컨트롤 변경(값>0) -> "midi:cc:<번호>", 프로그램 변경 -> "midi:pc:<번호>"

#include <QObject>
#include <QStringList>

namespace led {

class MidiInput : public QObject {
    Q_OBJECT

public:
    explicit MidiInput(QObject* parent = nullptr);
    ~MidiInput() override;

    static QStringList deviceNames();

    bool open(const QString& deviceName, QString* error);
    void close();
    bool isOpen() const { return m_handle != nullptr; }

    // winmm 콜백(다른 스레드)에서 호출
    void deliver(unsigned status, unsigned data1, unsigned data2);

signals:
    void triggered(const QString& trigger);

private:
    void* m_handle = nullptr;
};

} // namespace led
