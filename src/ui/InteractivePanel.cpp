#include "ui/InteractivePanel.h"

#include "core/Project.h"
#include "interactive/MidiInput.h"
#include "ui/ProjectController.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSerialPortInfo>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

namespace led {

namespace {

QSpinBox* makePort(QWidget* parent)
{
    auto* box = new QSpinBox(parent);
    box->setRange(1, 65535);
    box->setKeyboardTracking(false);
    return box;
}

const char kHelp[] =
    "명령 (한 줄에 하나, 영문/한글 모두 가능, 이름에 공백이 있으면 \"따옴표\")\n"
    "  scene <장면 이름|번호>   장면 전환            play / stop   재생 / 정지\n"
    "  next <영역> / prev <영역>  플레이리스트 다음/이전 항목\n"
    "  text <영역> <문구>       자막 변경 ('|'로 여러 문구)\n"
    "  show <영역> / hide <영역>  영역 표시/숨김\n"
    "  trigger <이름>           아래 연결표의 명령 실행\n"
    "  reset                    외부 명령으로 바꾼 자막/표시 되돌림\n"
    "  pause / go / seek <시각>  타임라인 일시정지 / 계속(큐 대기 해제) / 위치 이동 (예: 1:30)\n"
    "OSC: /scene \"오전\", /next \"메인\", /text \"하단\" \"문구\" 형식\n"
    "외부 명령으로 바꾼 자막/표시는 프로젝트에 저장되지 않습니다.";

} // namespace

InteractivePanel::InteractivePanel(ProjectController* controller, QWidget* parent)
    : QWidget(parent)
    , m_controller(controller)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    outer->addWidget(scroll);
    auto* body = new QWidget(scroll);
    scroll->setWidget(body);
    auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // ---- 외부 입력
    auto* inputs = new QGroupBox(QStringLiteral("외부 입력"), body);
    auto* grid = new QGridLayout(inputs);
    int row = 0;

    m_udpEnabled = new QCheckBox(QStringLiteral("UDP"), inputs);
    m_udpPort = makePort(inputs);
    grid->addWidget(m_udpEnabled, row, 0);
    grid->addWidget(new QLabel(QStringLiteral("포트"), inputs), row, 1);
    grid->addWidget(m_udpPort, row++, 2);

    m_tcpEnabled = new QCheckBox(QStringLiteral("TCP"), inputs);
    m_tcpPort = makePort(inputs);
    grid->addWidget(m_tcpEnabled, row, 0);
    grid->addWidget(new QLabel(QStringLiteral("포트"), inputs), row, 1);
    grid->addWidget(m_tcpPort, row++, 2);

    m_oscEnabled = new QCheckBox(QStringLiteral("OSC"), inputs);
    m_oscPort = makePort(inputs);
    grid->addWidget(m_oscEnabled, row, 0);
    grid->addWidget(new QLabel(QStringLiteral("포트"), inputs), row, 1);
    grid->addWidget(m_oscPort, row++, 2);

    m_serialEnabled = new QCheckBox(QStringLiteral("시리얼"), inputs);
    m_serialPort = new QComboBox(inputs);
    m_serialPort->setEditable(true);
    m_serialBaud = new QComboBox(inputs);
    for (int baud : {9600, 19200, 38400, 57600, 115200})
        m_serialBaud->addItem(QString::number(baud), baud);
    auto* serialRow = new QHBoxLayout;
    serialRow->addWidget(m_serialPort, 1);
    serialRow->addWidget(m_serialBaud);
    grid->addWidget(m_serialEnabled, row, 0);
    grid->addWidget(new QLabel(QStringLiteral("포트"), inputs), row, 1);
    grid->addLayout(serialRow, row++, 2);

    m_midiEnabled = new QCheckBox(QStringLiteral("MIDI"), inputs);
    m_midiDevice = new QComboBox(inputs);
    m_midiDevice->setEditable(true);
    grid->addWidget(m_midiEnabled, row, 0);
    grid->addWidget(new QLabel(QStringLiteral("장치"), inputs), row, 1);
    grid->addWidget(m_midiDevice, row++, 2);

    m_touchEnabled = new QCheckBox(QStringLiteral("출력 화면 터치/클릭 → 트리거 \"touch:<영역 이름>\""), inputs);
    grid->addWidget(m_touchEnabled, row++, 0, 1, 3);

    auto* refresh = new QPushButton(QStringLiteral("포트/장치 다시 찾기"), inputs);
    grid->addWidget(refresh, row++, 0, 1, 3);
    grid->setColumnStretch(2, 1);

    m_status = new QLabel(inputs);
    m_status->setObjectName("hintLabel");
    m_status->setWordWrap(true);
    grid->addWidget(m_status, row++, 0, 1, 3);

    auto* security = new QLabel(QStringLiteral("네트워크 입력(UDP/TCP/OSC)을 켜면 같은 네트워크의 누구나 명령을 보낼 수 있습니다. "
                                               "신뢰할 수 있는 네트워크에서만 사용하세요."),
                                inputs);
    security->setObjectName("hintLabel");
    security->setWordWrap(true);
    grid->addWidget(security, row++, 0, 1, 3);
    layout->addWidget(inputs);

    // ---- 트리거 연결표
    auto* mapGroup = new QGroupBox(QStringLiteral("트리거 연결 (터치/MIDI/trigger 명령 → 실행할 명령)"), body);
    auto* mapLayout = new QVBoxLayout(mapGroup);
    m_mappings = new QTableWidget(0, 2, mapGroup);
    m_mappings->setHorizontalHeaderLabels({QStringLiteral("트리거"), QStringLiteral("명령")});
    m_mappings->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_mappings->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_mappings->setColumnWidth(0, 170);
    m_mappings->verticalHeader()->setVisible(false);
    m_mappings->setMinimumHeight(150);
    mapLayout->addWidget(m_mappings);

    auto* mapButtons = new QHBoxLayout;
    auto* add = new QPushButton(QStringLiteral("추가"), mapGroup);
    auto* remove = new QPushButton(QStringLiteral("삭제"), mapGroup);
    m_useLastTrigger = new QPushButton(QStringLiteral("마지막 트리거로 추가"), mapGroup);
    m_useLastTrigger->setEnabled(false);
    mapButtons->addWidget(add);
    mapButtons->addWidget(remove);
    mapButtons->addStretch(1);
    mapButtons->addWidget(m_useLastTrigger);
    mapLayout->addLayout(mapButtons);

    m_lastTrigger = new QLabel(QStringLiteral("마지막 트리거: 없음 (MIDI 버튼을 누르거나 출력 화면을 터치하면 표시)"), mapGroup);
    m_lastTrigger->setObjectName("hintLabel");
    m_lastTrigger->setWordWrap(true);
    mapLayout->addWidget(m_lastTrigger);
    layout->addWidget(mapGroup);

    // ---- 명령 시험
    auto* testGroup = new QGroupBox(QStringLiteral("명령 시험"), body);
    auto* testLayout = new QVBoxLayout(testGroup);
    auto* testRow = new QHBoxLayout;
    m_testCommand = new QLineEdit(testGroup);
    m_testCommand->setPlaceholderText(QStringLiteral("예: scene \"오전 장면\""));
    auto* run = new QPushButton(QStringLiteral("실행"), testGroup);
    testRow->addWidget(m_testCommand, 1);
    testRow->addWidget(run);
    testLayout->addLayout(testRow);
    m_testResult = new QLabel(testGroup);
    m_testResult->setObjectName("hintLabel");
    m_testResult->setWordWrap(true);
    testLayout->addWidget(m_testResult);
    auto* help = new QLabel(QString::fromUtf8(kHelp), testGroup);
    help->setObjectName("hintLabel");
    help->setWordWrap(true);
    help->setTextInteractionFlags(Qt::TextSelectableByMouse);
    testLayout->addWidget(help);
    layout->addWidget(testGroup);
    layout->addStretch(1);

    // ---- 연결
    for (QCheckBox* c : {m_udpEnabled, m_tcpEnabled, m_oscEnabled, m_serialEnabled, m_midiEnabled, m_touchEnabled})
        connect(c, &QCheckBox::toggled, this, &InteractivePanel::commit);
    for (QSpinBox* s : {m_udpPort, m_tcpPort, m_oscPort})
        connect(s, QOverload<int>::of(&QSpinBox::valueChanged), this, &InteractivePanel::commit);
    for (QComboBox* c : {m_serialPort, m_serialBaud, m_midiDevice})
        connect(c, &QComboBox::currentTextChanged, this, &InteractivePanel::commit);
    connect(m_mappings, &QTableWidget::itemChanged, this, &InteractivePanel::commit);
    connect(refresh, &QPushButton::clicked, this, &InteractivePanel::refreshPorts);
    connect(add, &QPushButton::clicked, this, &InteractivePanel::addMapping);
    connect(remove, &QPushButton::clicked, this, &InteractivePanel::removeMapping);
    connect(m_useLastTrigger, &QPushButton::clicked, this, [this] {
        if (m_lastTriggerName.isEmpty())
            return;
        const int r = m_mappings->rowCount();
        m_loading = true;
        m_mappings->insertRow(r);
        m_mappings->setItem(r, 0, new QTableWidgetItem(m_lastTriggerName));
        m_mappings->setItem(r, 1, new QTableWidgetItem(QString()));
        m_loading = false;
        commit();
        m_mappings->setCurrentCell(r, 1);
        m_mappings->editItem(m_mappings->item(r, 1));
    });
    auto runCommand = [this] {
        const QString cmd = m_testCommand->text().trimmed();
        if (!cmd.isEmpty())
            emit commandRequested(cmd);
    };
    connect(run, &QPushButton::clicked, this, runCommand);
    connect(m_testCommand, &QLineEdit::returnPressed, this, runCommand);

    connect(controller, &ProjectController::projectReset, this, &InteractivePanel::load);
    connect(controller, &ProjectController::interactiveChanged, this, &InteractivePanel::load);

    refreshPorts();
    load();
}

void InteractivePanel::refreshPorts()
{
    const QString serial = m_serialPort->currentText();
    const QString midi = m_midiDevice->currentText();
    const bool wasLoading = m_loading;
    m_loading = true;
    m_serialPort->clear();
    for (const QSerialPortInfo& info : QSerialPortInfo::availablePorts()) {
        QString label = info.portName();
        m_serialPort->addItem(label);
    }
    m_serialPort->setCurrentText(serial);
    m_midiDevice->clear();
    m_midiDevice->addItems(MidiInput::deviceNames());
    m_midiDevice->setCurrentText(midi);
    m_loading = wasLoading;
}

void InteractivePanel::load()
{
    const InteractiveConfig& c = m_controller->project().interactive;
    m_loading = true;
    m_udpEnabled->setChecked(c.udpEnabled);
    m_udpPort->setValue(c.udpPort);
    m_tcpEnabled->setChecked(c.tcpEnabled);
    m_tcpPort->setValue(c.tcpPort);
    m_oscEnabled->setChecked(c.oscEnabled);
    m_oscPort->setValue(c.oscPort);
    m_serialEnabled->setChecked(c.serialEnabled);
    m_serialPort->setCurrentText(QString::fromStdString(c.serialPort));
    const int baudIndex = m_serialBaud->findData(c.serialBaud);
    if (baudIndex >= 0) {
        m_serialBaud->setCurrentIndex(baudIndex);
    } else {
        m_serialBaud->addItem(QString::number(c.serialBaud), c.serialBaud);
        m_serialBaud->setCurrentIndex(m_serialBaud->count() - 1);
    }
    m_midiEnabled->setChecked(c.midiEnabled);
    m_midiDevice->setCurrentText(QString::fromStdString(c.midiDevice));
    m_touchEnabled->setChecked(c.touchEnabled);

    // 이 화면에서 바꾼 경우에는 표를 다시 만들지 않음 (입력 중인 칸 유지)
    if (!m_committing) {
        const QSignalBlocker blocker(m_mappings);
        m_mappings->setRowCount(0);
        for (const TriggerMapping& m : c.mappings) {
            const int r = m_mappings->rowCount();
            m_mappings->insertRow(r);
            m_mappings->setItem(r, 0, new QTableWidgetItem(QString::fromStdString(m.trigger)));
            m_mappings->setItem(r, 1, new QTableWidgetItem(QString::fromStdString(m.command)));
        }
    }
    m_loading = false;
}

void InteractivePanel::commit()
{
    if (m_loading)
        return;
    InteractiveConfig c = m_controller->project().interactive;
    c.udpEnabled = m_udpEnabled->isChecked();
    c.udpPort = m_udpPort->value();
    c.tcpEnabled = m_tcpEnabled->isChecked();
    c.tcpPort = m_tcpPort->value();
    c.oscEnabled = m_oscEnabled->isChecked();
    c.oscPort = m_oscPort->value();
    c.serialEnabled = m_serialEnabled->isChecked();
    c.serialPort = m_serialPort->currentText().trimmed().toStdString();
    c.serialBaud = m_serialBaud->currentText().toInt();
    if (c.serialBaud <= 0)
        c.serialBaud = 9600;
    c.midiEnabled = m_midiEnabled->isChecked();
    c.midiDevice = m_midiDevice->currentText().trimmed().toStdString();
    c.touchEnabled = m_touchEnabled->isChecked();

    c.mappings.clear();
    for (int r = 0; r < m_mappings->rowCount(); ++r) {
        const QTableWidgetItem* t = m_mappings->item(r, 0);
        const QTableWidgetItem* cmd = m_mappings->item(r, 1);
        TriggerMapping m;
        m.trigger = t ? t->text().trimmed().toStdString() : std::string();
        m.command = cmd ? cmd->text().trimmed().toStdString() : std::string();
        c.mappings.push_back(m); // 빈 줄도 편집 중일 수 있어 유지 (저장 시 빈 트리거는 제외됨)
    }
    m_committing = true;
    m_controller->setInteractive(c);
    m_committing = false;
}

void InteractivePanel::addMapping()
{
    const int r = m_mappings->rowCount();
    m_loading = true;
    m_mappings->insertRow(r);
    m_mappings->setItem(r, 0, new QTableWidgetItem(QStringLiteral("touch:")));
    m_mappings->setItem(r, 1, new QTableWidgetItem(QString()));
    m_loading = false;
    commit();
    m_mappings->setCurrentCell(r, 0);
    m_mappings->editItem(m_mappings->item(r, 0));
}

void InteractivePanel::removeMapping()
{
    const int r = m_mappings->currentRow();
    if (r < 0)
        return;
    m_mappings->removeRow(r);
    commit();
}

void InteractivePanel::setStatusText(const QString& text)
{
    m_status->setText(text);
}

void InteractivePanel::setLastTrigger(const QString& trigger, const QString& source)
{
    m_lastTriggerName = trigger;
    m_lastTrigger->setText(QStringLiteral("마지막 트리거: %1  (%2)").arg(trigger, source));
    m_useLastTrigger->setEnabled(true);
}

void InteractivePanel::setCommandResult(const QString& text)
{
    m_testResult->setText(text);
}

} // namespace led
