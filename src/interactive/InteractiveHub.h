#pragma once

// 외부 입력 수신: UDP / TCP / OSC / 시리얼 / MIDI
//  - UDP, TCP, 시리얼: 한 줄에 명령 1개 (UTF-8)
//  - TCP: 명령마다 "OK" 또는 "ERR <사유>" 응답
//  - OSC: 주소+인수를 명령으로 변환 (예: /scene "오전" -> scene "오전")
//  - MIDI: 트리거 신호 (트리거 연결표에서 명령으로 바꿈)

#include "core/Project.h"

#include <QObject>
#include <QStringList>

#include <functional>

class QSerialPort;
class QTcpServer;
class QUdpSocket;

namespace led {

class MidiInput;

class InteractiveHub : public QObject {
    Q_OBJECT

public:
    // 명령 처리 함수: 성공이면 빈 문자열, 실패면 사유
    using Handler = std::function<QString(const QString& command, const QString& source)>;

    explicit InteractiveHub(QObject* parent = nullptr);
    ~InteractiveHub() override;

    void setHandler(Handler handler) { m_handler = std::move(handler); }

    // 설정에 맞게 수신을 다시 시작
    void apply(const InteractiveConfig& config);
    void stopAll();

    QString statusText() const { return m_status.join(QStringLiteral("\n")); }

    // 여러 줄 텍스트를 명령으로 나눠 처리 (검사용으로도 사용)
    void handleText(const QByteArray& data, const QString& source, QStringList* replies = nullptr);

signals:
    void triggerReceived(const QString& trigger, const QString& source);
    void statusChanged(const QString& text);

private:
    void startUdp(int port);
    void startTcp(int port);
    void startOsc(int port);
    void startSerial(const QString& port, int baud);
    void startMidi(const QString& device);
    void addStatus(const QString& text, bool ok);

    Handler m_handler;
    QUdpSocket* m_udp = nullptr;
    QUdpSocket* m_osc = nullptr;
    QTcpServer* m_tcp = nullptr;
    QSerialPort* m_serial = nullptr;
    QByteArray m_serialBuffer;
    MidiInput* m_midi = nullptr;
    QStringList m_status;
};

} // namespace led
