#include "ui/CanvasPanel.h"

#include "config/ProjectSerializer.h"
#include "media/AudioOutput.h"
#include "ui/FormHelpers.h"
#include "ui/ProjectController.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>

namespace led {

CanvasPanel::CanvasPanel(ProjectController* controller, QWidget* parent)
    : QWidget(parent)
    , m_controller(controller)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto* group = new QGroupBox(QStringLiteral("캔버스 (전체 해상도)"), this);
    auto* form = new QFormLayout(group);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_width = makePixelSpinBox(1, kMaxCanvasSize, group);
    m_height = makePixelSpinBox(1, kMaxCanvasSize, group);
    form->addRow(QStringLiteral("너비"), m_width);
    form->addRow(QStringLiteral("높이"), m_height);

    auto* fitOutputs = new QPushButton(QStringLiteral("출력 범위 전체에 맞춤"), group);
    auto* fitRegions = new QPushButton(QStringLiteral("영역 전체에 맞춤"), group);
    form->addRow(QString(), fitOutputs);
    form->addRow(QString(), fitRegions);
    layout->addWidget(group);

    auto* playback = new QGroupBox(QStringLiteral("재생 설정"), this);
    auto* playbackLayout = new QVBoxLayout(playback);
    m_hardwareDecoding = new QCheckBox(QStringLiteral("하드웨어 디코딩 사용 (그래픽카드로 영상 처리)"), playback);
    playbackLayout->addWidget(m_hardwareDecoding);
    auto* hwHint = new QLabel(QStringLiteral("CPU 부하를 줄여 고해상도 영상을 여러 개 재생할 때 유리합니다.\n"
                                             "지원하지 않는 영상/그래픽카드는 자동으로 CPU로 처리합니다.\n"
                                             "영상이 깨지거나 멈추면 끄고 확인하세요. 영역별 설정은 '영역' 탭."),
                              playback);
    hwHint->setObjectName("hintLabel");
    hwHint->setWordWrap(true);
    playbackLayout->addWidget(hwHint);

    auto* audioRow = new QHBoxLayout;
    audioRow->addWidget(new QLabel(QStringLiteral("소리 출력 장치"), playback));
    m_audioDevice = new QComboBox(playback);
    m_audioDevice->setMinimumWidth(200);
    auto* refreshAudio = new QPushButton(QStringLiteral("다시 찾기"), playback);
    audioRow->addWidget(m_audioDevice, 1);
    audioRow->addWidget(refreshAudio);
    playbackLayout->addLayout(audioRow);
    auto* audioHint = new QLabel(QStringLiteral("타임라인의 소리가 이 장치로 나옵니다. 지정한 장치가 없으면 기본 장치로 출력합니다."),
                                 playback);
    audioHint->setObjectName("hintLabel");
    audioHint->setWordWrap(true);
    playbackLayout->addWidget(audioHint);
    connect(refreshAudio, &QPushButton::clicked, this, &CanvasPanel::refreshAudioDevices);
    connect(controller, &ProjectController::projectReset, this, &CanvasPanel::refreshAudioDevices);
    connect(m_audioDevice, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
        if (!m_loading)
            m_controller->setAudioDevice(m_audioDevice->currentData().toString());
    });
    layout->addWidget(playback);

    m_summary = new QLabel(this);
    m_summary->setObjectName("hintLabel");
    m_summary->setWordWrap(true);
    layout->addWidget(m_summary);

    auto* hint = new QLabel(QStringLiteral("캔버스는 모든 영역과 출력이 놓이는 하나의 좌표 공간입니다.\n"
                                           "영역 좌표와 출력 범위는 모두 이 캔버스 기준 픽셀 값입니다."),
                            this);
    hint->setObjectName("hintLabel");
    hint->setWordWrap(true);
    layout->addWidget(hint);
    layout->addStretch(1);

    connect(m_width, QOverload<int>::of(&QSpinBox::valueChanged), this, &CanvasPanel::commit);
    connect(m_height, QOverload<int>::of(&QSpinBox::valueChanged), this, &CanvasPanel::commit);
    connect(m_hardwareDecoding, &QCheckBox::toggled, this, [this](bool on) {
        if (!m_loading)
            m_controller->setHardwareDecoding(on);
    });
    connect(controller, &ProjectController::playbackSettingsChanged, this, &CanvasPanel::load);
    connect(fitOutputs, &QPushButton::clicked, this, &CanvasPanel::fitToOutputs);
    connect(fitRegions, &QPushButton::clicked, this, &CanvasPanel::fitToRegions);

    connect(controller, &ProjectController::projectReset, this, &CanvasPanel::load);
    connect(controller, &ProjectController::canvasChanged, this, &CanvasPanel::load);
    connect(controller, &ProjectController::outputsChanged, this, &CanvasPanel::load);
    connect(controller, &ProjectController::regionsChanged, this, &CanvasPanel::load);
    refreshAudioDevices();
    load();
}

void CanvasPanel::load()
{
    const Project& p = m_controller->project();
    m_loading = true;
    m_width->setValue(p.canvas.width);
    m_height->setValue(p.canvas.height);
    m_hardwareDecoding->setChecked(p.hardwareDecoding);
    m_loading = false;
    selectAudioDevice();

    int enabled = 0;
    for (const auto& o : p.outputs)
        enabled += o.enabled ? 1 : 0;
    const double megapixels = double(p.canvas.width) * double(p.canvas.height) / 1'000'000.0;
    m_summary->setText(QStringLiteral("영역 %1개, 출력 %2개 (사용 %3개)\n캔버스 픽셀 수: %4 MP")
                           .arg(static_cast<int>(p.regions.size()))
                           .arg(static_cast<int>(p.outputs.size()))
                           .arg(enabled)
                           .arg(megapixels, 0, 'f', 2));
}

void CanvasPanel::refreshAudioDevices()
{
    std::string error;
    const std::vector<AudioDeviceInfo> devices = AudioOutput::instance().devices(&error);
    const bool wasLoading = m_loading;
    m_loading = true;
    m_audioDevice->clear();
    m_audioDevice->addItem(QStringLiteral("기본 장치 (Windows 설정)"), QString());
    for (const AudioDeviceInfo& d : devices) {
        const QString name = QString::fromStdString(d.name);
        m_audioDevice->addItem(d.isDefault ? name + QStringLiteral("  (기본)") : name, name);
    }
    m_audioDevice->setToolTip(error.empty() ? QString() : QString::fromStdString(error));
    m_loading = wasLoading;
    selectAudioDevice();
}

void CanvasPanel::selectAudioDevice()
{
    const QString wanted = QString::fromStdString(m_controller->project().audioDevice);
    const bool wasLoading = m_loading;
    m_loading = true;
    // 이전에 넣은 '(연결 안 됨)' 항목 정리
    for (int i = m_audioDevice->count() - 1; i > 0; --i)
        if (m_audioDevice->itemText(i).endsWith(QStringLiteral("(연결 안 됨)")))
            m_audioDevice->removeItem(i);
    int index = m_audioDevice->findData(wanted);
    if (index < 0) {
        m_audioDevice->addItem(QStringLiteral("%1  (연결 안 됨)").arg(wanted), wanted);
        index = m_audioDevice->count() - 1;
    }
    m_audioDevice->setCurrentIndex(index);
    m_loading = wasLoading;
}

void CanvasPanel::commit()
{
    if (m_loading)
        return;
    m_controller->setCanvasSize(m_width->value(), m_height->value());
}

void CanvasPanel::fitToOutputs()
{
    const Project& p = m_controller->project();
    int w = 0;
    int h = 0;
    for (const auto& o : p.outputs) {
        w = std::max(w, o.canvasRect.right());
        h = std::max(h, o.canvasRect.bottom());
    }
    if (w > 0 && h > 0)
        m_controller->setCanvasSize(w, h);
}

void CanvasPanel::fitToRegions()
{
    const Project& p = m_controller->project();
    int w = 0;
    int h = 0;
    for (const auto& r : p.regions) {
        w = std::max(w, r.rect.right());
        h = std::max(h, r.rect.bottom());
    }
    if (w > 0 && h > 0)
        m_controller->setCanvasSize(w, h);
}

} // namespace led
