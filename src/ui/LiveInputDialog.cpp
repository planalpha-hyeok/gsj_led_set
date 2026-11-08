#include "ui/LiveInputDialog.h"

#include "media/CaptureDevices.h"

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace led {

LiveInputDialog::LiveInputDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("라이브 입력"));
    setMinimumWidth(480);

    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;

    auto* deviceRow = new QHBoxLayout;
    m_device = new QComboBox(this);
    m_device->setEditable(true); // 목록에 없으면 이름 직접 입력
    m_device->setMinimumWidth(260);
    auto* refresh = new QPushButton(QStringLiteral("다시 찾기"), this);
    deviceRow->addWidget(m_device, 1);
    deviceRow->addWidget(refresh);
    form->addRow(QStringLiteral("입력 장치"), deviceRow);

    m_size = new QComboBox(this);
    m_size->setEditable(true);
    m_size->addItem(QStringLiteral("자동 (장치 기본값)"), QString());
    for (const char* s : {"3840x2160", "1920x1080", "1280x720", "720x480"})
        m_size->addItem(QString::fromLatin1(s), QString::fromLatin1(s));
    form->addRow(QStringLiteral("해상도"), m_size);

    m_fps = new QDoubleSpinBox(this);
    m_fps->setRange(0.0, 240.0);
    m_fps->setDecimals(2);
    m_fps->setSpecialValueText(QStringLiteral("자동 (장치 기본값)"));
    m_fps->setSuffix(QStringLiteral(" fps"));
    form->addRow(QStringLiteral("프레임"), m_fps);
    layout->addLayout(form);

    m_status = new QLabel(this);
    m_status->setObjectName("hintLabel");
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    auto* hint = new QLabel(QStringLiteral("장치가 지원하지 않는 해상도/프레임을 지정하면 열리지 않습니다. 모르면 '자동'.\n"
                                           "재생 중 장치가 끊기면 2초마다 다시 연결합니다."),
                            this);
    hint->setObjectName("hintLabel");
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("확인"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("취소"));
    layout->addWidget(buttons);

    connect(refresh, &QPushButton::clicked, this, &LiveInputDialog::refreshDevices);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        if (m_device->currentText().trimmed().isEmpty()) {
            m_status->setText(QStringLiteral("입력 장치를 선택하거나 이름을 입력하세요."));
            return;
        }
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    m_item.id = generateId();
    m_item.type = ItemType::Live;
    refreshDevices();
}

void LiveInputDialog::refreshDevices()
{
    const QString current = m_device->currentText();
    QApplication::setOverrideCursor(Qt::WaitCursor);
    std::string error;
    const std::vector<CaptureDevice> devices = listCaptureDevices(&error);
    QApplication::restoreOverrideCursor();

    m_device->clear();
    for (const CaptureDevice& d : devices)
        m_device->addItem(QString::fromStdString(d.name), QString::fromStdString(d.id));
    if (!current.isEmpty())
        m_device->setCurrentText(current);

    if (!error.empty())
        m_status->setText(QString::fromStdString(error) + QStringLiteral("\n장치 이름을 직접 입력할 수 있습니다."));
    else if (devices.empty())
        m_status->setText(QStringLiteral("찾은 입력 장치가 없습니다. 장치 연결과 드라이버를 확인하세요."));
    else
        m_status->setText(QStringLiteral("입력 장치 %1개").arg(static_cast<int>(devices.size())));
}

void LiveInputDialog::setItem(const PlaylistItem& item)
{
    m_item = item;
    m_device->setCurrentText(QString::fromStdString(item.source));
    const int sizeIndex = m_size->findData(QString::fromStdString(item.liveSize));
    if (sizeIndex >= 0)
        m_size->setCurrentIndex(sizeIndex);
    else
        m_size->setCurrentText(QString::fromStdString(item.liveSize));
    m_fps->setValue(item.liveFps);
}

PlaylistItem LiveInputDialog::item() const
{
    PlaylistItem out = m_item;
    out.type = ItemType::Live;
    // 목록에서 고른 경우 장치 id, 직접 입력한 경우 입력한 이름
    const int index = m_device->findText(m_device->currentText());
    out.source = (index >= 0 ? m_device->itemData(index).toString() : m_device->currentText().trimmed()).toStdString();
    // 해상도: 목록 항목이면 data, 직접 입력이면 텍스트 ("자동"은 빈 값)
    const int sizeIndex = m_size->findText(m_size->currentText());
    QString size = sizeIndex >= 0 ? m_size->itemData(sizeIndex).toString() : m_size->currentText().trimmed();
    out.liveSize = size.toStdString();
    out.liveFps = m_fps->value();
    return out;
}

} // namespace led
