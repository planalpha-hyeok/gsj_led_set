#include "ui/OutputPanel.h"

#include "core/Project.h"
#include "ui/FormHelpers.h"
#include "ui/ProjectController.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace led {

namespace {

constexpr int kCoordLimit = 1000000;

QString displayText(const DisplayInfo& d)
{
    QString text = QStringLiteral("%1  %2×%3").arg(QString::fromStdString(d.id)).arg(d.width).arg(d.height);
    if (d.primary)
        text += QStringLiteral("  (주 모니터)");
    return text;
}

QString displayDetail(const DisplayInfo& d)
{
    QString text = QStringLiteral("바탕화면 위치 %1, %2  /  %3×%4").arg(d.x).arg(d.y).arg(d.width).arg(d.height);
    if (d.refreshHz > 0)
        text += QStringLiteral("  /  %1Hz").arg(d.refreshHz);
    if (!d.adapter.empty())
        text += QStringLiteral("\n그래픽카드: %1").arg(QString::fromStdString(d.adapter));
    if (!d.name.empty())
        text += QStringLiteral("\n장치: %1").arg(QString::fromStdString(d.name));
    return text;
}

} // namespace

OutputPanel::OutputPanel(ProjectController* controller, QWidget* parent)
    : QWidget(parent)
    , m_controller(controller)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto* listLabel = new QLabel(QStringLiteral("출력 목록 (그래픽카드 출력 1개 = 출력 1개)"), this);
    listLabel->setObjectName("sectionLabel");
    layout->addWidget(listLabel);

    m_list = new QListWidget(this);
    m_list->setMinimumHeight(100);
    layout->addWidget(m_list, 1);

    auto* buttons = new QHBoxLayout;
    m_addButton = new QPushButton(QStringLiteral("추가"), this);
    m_removeButton = new QPushButton(QStringLiteral("삭제"), this);
    m_refreshButton = new QPushButton(QStringLiteral("디스플레이 다시 찾기"), this);
    buttons->addWidget(m_addButton);
    buttons->addWidget(m_removeButton);
    buttons->addStretch(1);
    buttons->addWidget(m_refreshButton);
    layout->addLayout(buttons);

    m_form = new QGroupBox(QStringLiteral("출력 속성"), this);
    auto* form = new QFormLayout(m_form);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_name = new QLineEdit(m_form);
    form->addRow(QStringLiteral("이름"), m_name);

    m_display = new QComboBox(m_form);
    form->addRow(QStringLiteral("디스플레이"), m_display);
    m_displayInfo = new QLabel(m_form);
    m_displayInfo->setObjectName("hintLabel");
    m_displayInfo->setWordWrap(true);
    form->addRow(QString(), m_displayInfo);

    auto* canvasGrid = new QGridLayout;
    m_x = makePixelSpinBox(-kCoordLimit, kCoordLimit, m_form);
    m_y = makePixelSpinBox(-kCoordLimit, kCoordLimit, m_form);
    m_w = makePixelSpinBox(1, kCoordLimit, m_form);
    m_h = makePixelSpinBox(1, kCoordLimit, m_form);
    canvasGrid->addWidget(new QLabel(QStringLiteral("X"), m_form), 0, 0);
    canvasGrid->addWidget(m_x, 0, 1);
    canvasGrid->addWidget(new QLabel(QStringLiteral("Y"), m_form), 0, 2);
    canvasGrid->addWidget(m_y, 0, 3);
    canvasGrid->addWidget(new QLabel(QStringLiteral("너비"), m_form), 1, 0);
    canvasGrid->addWidget(m_w, 1, 1);
    canvasGrid->addWidget(new QLabel(QStringLiteral("높이"), m_form), 1, 2);
    canvasGrid->addWidget(m_h, 1, 3);
    form->addRow(QStringLiteral("캔버스 범위"), canvasGrid);

    m_displaySizeButton = new QPushButton(QStringLiteral("크기를 디스플레이 해상도로 설정"), m_form);
    form->addRow(QString(), m_displaySizeButton);

    auto* offsetRow = new QHBoxLayout;
    m_offsetX = makePixelSpinBox(-kCoordLimit, kCoordLimit, m_form);
    m_offsetY = makePixelSpinBox(-kCoordLimit, kCoordLimit, m_form);
    offsetRow->addWidget(new QLabel(QStringLiteral("X"), m_form));
    offsetRow->addWidget(m_offsetX);
    offsetRow->addWidget(new QLabel(QStringLiteral("Y"), m_form));
    offsetRow->addWidget(m_offsetY);
    offsetRow->addStretch(1);
    form->addRow(QStringLiteral("디스플레이 내 위치"), offsetRow);

    m_enabled = new QCheckBox(QStringLiteral("이 출력 사용"), m_form);
    form->addRow(QString(), m_enabled);

    auto* hint = new QLabel(QStringLiteral("출력 창은 캔버스 범위를 1:1 픽셀로 내보냅니다.\n"
                                           "송출카드(NovaLCT / LEDVISION) 매핑 좌표와 같은 값으로 맞추세요."),
                            m_form);
    hint->setObjectName("hintLabel");
    hint->setWordWrap(true);
    form->addRow(QString(), hint);

    layout->addWidget(m_form);

    connect(m_addButton, &QPushButton::clicked, this, [this] {
        m_selectedId = m_controller->addOutput();
        refreshList();
    });
    connect(m_removeButton, &QPushButton::clicked, this, [this] {
        if (!m_selectedId.isEmpty())
            m_controller->removeOutput(m_selectedId);
    });
    connect(m_refreshButton, &QPushButton::clicked, this, &OutputPanel::refreshDisplays);
    connect(m_list, &QListWidget::itemSelectionChanged, this, [this] {
        const auto items = m_list->selectedItems();
        if (items.isEmpty())
            return;
        m_selectedId = items.first()->data(Qt::UserRole).toString();
        loadSelected();
    });

    connect(m_name, &QLineEdit::editingFinished, this, &OutputPanel::commit);
    connect(m_display, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OutputPanel::commit);
    for (QSpinBox* box : {m_x, m_y, m_w, m_h, m_offsetX, m_offsetY})
        connect(box, QOverload<int>::of(&QSpinBox::valueChanged), this, &OutputPanel::commit);
    connect(m_enabled, &QCheckBox::toggled, this, &OutputPanel::commit);
    connect(m_displaySizeButton, &QPushButton::clicked, this, &OutputPanel::applyDisplaySize);

    connect(controller, &ProjectController::projectReset, this, [this] {
        m_selectedId.clear();
        refreshList();
    });
    connect(controller, &ProjectController::outputsChanged, this, &OutputPanel::refreshList);

    m_displays = enumerateDisplays();
    refreshList();
}

void OutputPanel::refreshDisplays()
{
    m_displays = enumerateDisplays();
    refreshList();
}

void OutputPanel::refreshList()
{
    const Project& project = m_controller->project();
    if (!m_selectedId.isEmpty() && !project.findOutput(m_selectedId.toStdString()))
        m_selectedId.clear();
    if (m_selectedId.isEmpty() && !project.outputs.empty())
        m_selectedId = QString::fromStdString(project.outputs.front().id);

    {
        const QSignalBlocker blocker(m_list);
        m_list->clear();
        int index = 0;
        for (const OutputConfig& o : project.outputs) {
            ++index;
            const DisplayInfo* d = findDisplay(m_displays, o.displayId);
            QString text = QStringLiteral("%1. %2   [%3]   캔버스 %4, %5  %6×%7")
                               .arg(index)
                               .arg(QString::fromStdString(o.name))
                               .arg(d ? QString::fromStdString(d->id) : QStringLiteral("연결 안 됨"))
                               .arg(o.canvasRect.x)
                               .arg(o.canvasRect.y)
                               .arg(o.canvasRect.w)
                               .arg(o.canvasRect.h);
            if (!o.enabled)
                text += QStringLiteral("  [사용 안 함]");
            auto* item = new QListWidgetItem(text, m_list);
            item->setData(Qt::UserRole, QString::fromStdString(o.id));
            if (!d || !o.enabled)
                item->setForeground(QColor(150, 150, 150));
            if (QString::fromStdString(o.id) == m_selectedId) {
                m_list->setCurrentItem(item);
                item->setSelected(true);
            }
        }
    }
    loadSelected();
}

void OutputPanel::fillDisplayCombo(const std::string& currentId)
{
    const QSignalBlocker blocker(m_display);
    m_display->clear();
    int currentIndex = -1;
    for (const DisplayInfo& d : m_displays) {
        m_display->addItem(displayText(d), QString::fromStdString(d.id));
        if (d.id == currentId)
            currentIndex = m_display->count() - 1;
    }
    if (currentIndex < 0) {
        const QString label = currentId.empty() ? QStringLiteral("(선택 안 됨)")
                                                : QStringLiteral("%1  (연결 안 됨)").arg(QString::fromStdString(currentId));
        m_display->addItem(label, QString::fromStdString(currentId));
        currentIndex = m_display->count() - 1;
    }
    m_display->setCurrentIndex(currentIndex);
}

void OutputPanel::loadSelected()
{
    const OutputConfig* o = m_controller->project().findOutput(m_selectedId.toStdString());
    m_form->setEnabled(o != nullptr);
    m_removeButton->setEnabled(o != nullptr);
    if (!o)
        return;

    m_loading = true;
    m_name->setText(QString::fromStdString(o->name));
    fillDisplayCombo(o->displayId);
    const DisplayInfo* d = findDisplay(m_displays, o->displayId);
    m_displayInfo->setText(d ? displayDetail(*d) : QStringLiteral("이 디스플레이가 현재 연결되어 있지 않습니다."));
    m_displaySizeButton->setEnabled(d != nullptr);
    m_x->setValue(o->canvasRect.x);
    m_y->setValue(o->canvasRect.y);
    m_w->setValue(o->canvasRect.w);
    m_h->setValue(o->canvasRect.h);
    m_offsetX->setValue(o->offsetX);
    m_offsetY->setValue(o->offsetY);
    m_enabled->setChecked(o->enabled);
    m_loading = false;
}

void OutputPanel::commit()
{
    if (m_loading)
        return;
    const OutputConfig* current = m_controller->project().findOutput(m_selectedId.toStdString());
    if (!current)
        return;
    OutputConfig o = *current;
    o.name = m_name->text().trimmed().toStdString();
    if (o.name.empty())
        o.name = current->name;
    o.displayId = m_display->currentData().toString().toStdString();
    o.canvasRect = RectI{m_x->value(), m_y->value(), m_w->value(), m_h->value()};
    o.offsetX = m_offsetX->value();
    o.offsetY = m_offsetY->value();
    o.enabled = m_enabled->isChecked();

    const bool changed = o.name != current->name || o.displayId != current->displayId ||
                         o.canvasRect != current->canvasRect || o.offsetX != current->offsetX ||
                         o.offsetY != current->offsetY || o.enabled != current->enabled;
    if (changed)
        m_controller->updateOutput(o);
}

void OutputPanel::applyDisplaySize()
{
    const OutputConfig* current = m_controller->project().findOutput(m_selectedId.toStdString());
    if (!current)
        return;
    const DisplayInfo* d = findDisplay(m_displays, current->displayId);
    if (!d)
        return;
    OutputConfig o = *current;
    o.canvasRect.w = d->width;
    o.canvasRect.h = d->height;
    o.offsetX = 0;
    o.offsetY = 0;
    m_controller->updateOutput(o);
}

} // namespace led
