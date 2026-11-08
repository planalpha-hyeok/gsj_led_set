#include "interactive/InteractiveHub.h"

#include "core/Log.h"
#include "core/OscParser.h"
#include "interactive/MidiInput.h"

#include <QHostAddress>
#include <QNetworkDatagram>
#include <QRegularExpression>
#include <QSerialPort>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>

namespace led {

namespace {
constexpr qint64 kMaxLineBytes = 64 * 1024; // 줄바꿈 없이 이보다 길면 연결 끊음
}

InteractiveHub::InteractiveHub(QObject* parent)
    : QObject(parent)
{
}

InteractiveHub::~InteractiveHub()
{
    stopAll();
}

void InteractiveHub::stopAll()
{
    delete m_udp;
    m_udp = nullptr;
    delete m_osc;
    m_osc = nullptr;
    if (m_tcp) {
        m_tcp->close();
        delete m_tcp; // 연결된 소켓은 서버의 자식이라 함께 정리
        m_tcp = nullptr;
    }
    if (m_serial) {
        m_serial->close();
        delete m_serial;
        m_serial = nullptr;
    }
    m_serialBuffer.clear();
    if (m_midi) {
        m_midi->close();
        delete m_midi;
        m_midi = nullptr;
    }
    m_status.clear();
}

void InteractiveHub::addStatus(const QString& text, bool ok)
{
    m_status << text;
    if (ok)
        logInfo("인터랙티브: " + text.toStdString());
    else
        logError("인터랙티브: " + text.toStdString());
}

void InteractiveHub::apply(const InteractiveConfig& c)
{
    stopAll();
    if (c.udpEnabled)
        startUdp(c.udpPort);
    if (c.tcpEnabled)
        startTcp(c.tcpPort);
    if (c.oscEnabled)
        startOsc(c.oscPort);
    if (c.serialEnabled)
        startSerial(QString::fromStdString(c.serialPort), c.serialBaud);
    if (c.midiEnabled)
        startMidi(QString::fromStdString(c.midiDevice));
    if (m_status.isEmpty())
        m_status << QStringLiteral("외부 입력 사용 안 함");
    emit statusChanged(statusText());
}

void InteractiveHub::handleText(const QByteArray& data, const QString& source, QStringList* replies)
{
    const QString text = QString::fromUtf8(data);
    for (const QString& raw : text.split(QRegularExpression(QStringLiteral("[\\r\\n]")), Qt::SkipEmptyParts)) {
        const QString line = raw.trimmed();
        if (line.isEmpty())
            continue;
        const QString error = m_handler ? m_handler(line, source) : QStringLiteral("처리기 없음");
        if (replies)
            *replies << (error.isEmpty() ? QStringLiteral("OK") : QStringLiteral("ERR ") + error);
    }
}

void InteractiveHub::startUdp(int port)
{
    m_udp = new QUdpSocket(this);
    if (!m_udp->bind(QHostAddress::AnyIPv4, static_cast<quint16>(port))) {
        addStatus(QStringLiteral("UDP %1 포트 열기 실패 (%2)").arg(port).arg(m_udp->errorString()), false);
        delete m_udp;
        m_udp = nullptr;
        return;
    }
    connect(m_udp, &QUdpSocket::readyRead, this, [this] {
        while (m_udp && m_udp->hasPendingDatagrams()) {
            const QNetworkDatagram d = m_udp->receiveDatagram();
            handleText(d.data(), QStringLiteral("UDP %1").arg(d.senderAddress().toString()));
        }
    });
    addStatus(QStringLiteral("UDP %1 포트 수신 중").arg(port), true);
}

void InteractiveHub::startOsc(int port)
{
    m_osc = new QUdpSocket(this);
    if (!m_osc->bind(QHostAddress::AnyIPv4, static_cast<quint16>(port))) {
        addStatus(QStringLiteral("OSC %1 포트 열기 실패 (%2)").arg(port).arg(m_osc->errorString()), false);
        delete m_osc;
        m_osc = nullptr;
        return;
    }
    connect(m_osc, &QUdpSocket::readyRead, this, [this] {
        while (m_osc && m_osc->hasPendingDatagrams()) {
            const QNetworkDatagram d = m_osc->receiveDatagram();
            const QByteArray bytes = d.data();
            const QString source = QStringLiteral("OSC %1").arg(d.senderAddress().toString());
            for (const OscMessage& m :
                 parseOscPacket(reinterpret_cast<const std::uint8_t*>(bytes.constData()), static_cast<std::size_t>(bytes.size())))
                handleText(QByteArray::fromStdString(oscToCommand(m)), source);
        }
    });
    addStatus(QStringLiteral("OSC %1 포트 수신 중").arg(port), true);
}

void InteractiveHub::startTcp(int port)
{
    m_tcp = new QTcpServer(this);
    if (!m_tcp->listen(QHostAddress::AnyIPv4, static_cast<quint16>(port))) {
        addStatus(QStringLiteral("TCP %1 포트 열기 실패 (%2)").arg(port).arg(m_tcp->errorString()), false);
        delete m_tcp;
        m_tcp = nullptr;
        return;
    }
    connect(m_tcp, &QTcpServer::newConnection, this, [this] {
        while (m_tcp && m_tcp->hasPendingConnections()) {
            QTcpSocket* socket = m_tcp->nextPendingConnection();
            socket->setParent(m_tcp);
            const QString source = QStringLiteral("TCP %1").arg(socket->peerAddress().toString());
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QTcpSocket::readyRead, this, [this, socket, source] {
                while (socket->canReadLine()) {
                    QStringList replies;
                    handleText(socket->readLine(), source, &replies);
                    for (const QString& r : replies)
                        socket->write((r + QLatin1Char('\n')).toUtf8());
                }
                if (socket->bytesAvailable() > kMaxLineBytes) {
                    logWarning("TCP: 줄바꿈 없는 긴 데이터라 연결을 끊음");
                    socket->abort();
                }
            });
        }
    });
    addStatus(QStringLiteral("TCP %1 포트 수신 중").arg(port), true);
}

void InteractiveHub::startSerial(const QString& port, int baud)
{
    if (port.isEmpty()) {
        addStatus(QStringLiteral("시리얼 포트가 지정되지 않음"), false);
        return;
    }
    m_serial = new QSerialPort(this);
    m_serial->setPortName(port);
    m_serial->setBaudRate(baud);
    if (!m_serial->open(QIODevice::ReadOnly)) {
        addStatus(QStringLiteral("시리얼 %1 열기 실패 (%2)").arg(port, m_serial->errorString()), false);
        delete m_serial;
        m_serial = nullptr;
        return;
    }
    connect(m_serial, &QSerialPort::readyRead, this, [this, port] {
        m_serialBuffer += m_serial->readAll();
        // 마지막 줄바꿈까지 처리하고 나머지는 다음 수신과 합침
        int last = -1;
        for (int i = m_serialBuffer.size() - 1; i >= 0; --i)
            if (m_serialBuffer[i] == '\n' || m_serialBuffer[i] == '\r') {
                last = i;
                break;
            }
        if (last >= 0) {
            const QByteArray complete = m_serialBuffer.left(last + 1);
            m_serialBuffer.remove(0, last + 1);
            handleText(complete, QStringLiteral("시리얼 %1").arg(port));
        }
        if (m_serialBuffer.size() > kMaxLineBytes)
            m_serialBuffer.clear();
    });
    addStatus(QStringLiteral("시리얼 %1 (%2bps) 수신 중").arg(port).arg(baud), true);
}

void InteractiveHub::startMidi(const QString& device)
{
    m_midi = new MidiInput(this);
    QString error;
    if (!m_midi->open(device, &error)) {
        addStatus(error, false);
        delete m_midi;
        m_midi = nullptr;
        return;
    }
    connect(m_midi, &MidiInput::triggered, this,
            [this, device](const QString& trigger) { emit triggerReceived(trigger, QStringLiteral("MIDI ") + device); });
    addStatus(QStringLiteral("MIDI '%1' 수신 중").arg(device), true);
}

} // namespace led
