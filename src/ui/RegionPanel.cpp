#include "ui/RegionPanel.h"

#include "core/Project.h"
#include "ui/ContentEditor.h"
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
#include <QScrollArea>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>

namespace led {

namespace {

constexpr int kCoordLimit = 1000000;

QString contentSummary(const RegionContent* c)
{
    if (!c || c->isEmpty())
        return QStringLiteral("콘텐츠 없음");
    if (c->kind == ContentKind::Ticker)
        return QStringLiteral("자막");
    return QStringLiteral("항목 %1개").arg(static_cast<int>(c->items.size()));
}

} // namespace

RegionPanel::RegionPanel(ProjectController* controller, QWidget* parent)
    : QWidget(parent)
    , m_controller(controller)
{
    // 내용이 길어 스크롤 영역 안에 배치
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); // 패널 폭에 맞춤
    outer->addWidget(scroll);
    auto* body = new QWidget(scroll);
    scroll->setWidget(body);

    auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto* listLabel = new QLabel(QStringLiteral("영역 목록 (위쪽이 화면 앞)"), body);
    listLabel->setObjectName("sectionLabel");
    layout->addWidget(listLabel);

    m_list = new QListWidget(body);
    m_list->setMinimumHeight(100);
    m_list->setMaximumHeight(170);
    layout->addWidget(m_list);

    auto* buttons = new QHBoxLayout;
    m_addButton = new QPushButton(QStringLiteral("추가"), body);
    m_removeButton = new QPushButton(QStringLiteral("삭제"), body);
    m_upButton = new QPushButton(QStringLiteral("앞으로"), body);
    m_downButton = new QPushButton(QStringLiteral("뒤로"), body);
    for (QPushButton* b : {m_addButton, m_removeButton, m_upButton, m_downButton})
        buttons->addWidget(b);
    layout->addLayout(buttons);

    // 위치/표시 (모든 장면 공통)
    auto* group = new QGroupBox(QStringLiteral("영역 위치/표시 (모든 장면 공통)"), body);
    m_form = group;
    auto* form = new QFormLayout(group);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_name = new QLineEdit(group);
    form->addRow(QStringLiteral("이름"), m_name);

    auto* geometry = new QGridLayout;
    m_x = makePixelSpinBox(-kCoordLimit, kCoordLimit, group);
    m_y = makePixelSpinBox(-kCoordLimit, kCoordLimit, group);
    m_w = makePixelSpinBox(1, kCoordLimit, group);
    m_h = makePixelSpinBox(1, kCoordLimit, group);
    geometry->addWidget(new QLabel(QStringLiteral("X"), group), 0, 0);
    geometry->addWidget(m_x, 0, 1);
    geometry->addWidget(new QLabel(QStringLiteral("Y"), group), 0, 2);
    geometry->addWidget(m_y, 0, 3);
    geometry->addWidget(new QLabel(QStringLiteral("너비"), group), 1, 0);
    geometry->addWidget(m_w, 1, 1);
    geometry->addWidget(new QLabel(QStringLiteral("높이"), group), 1, 2);
    geometry->addWidget(m_h, 1, 3);
    form->addRow(QStringLiteral("위치/크기"), geometry);

    m_fit = new QComboBox(group);
    m_fit->addItem(QStringLiteral("늘리기 (영역에 꽉 맞춤)"), int(FitMode::Stretch));
    m_fit->addItem(QStringLiteral("맞추기 (비율 유지, 여백)"), int(FitMode::Fit));
    m_fit->addItem(QStringLiteral("채우기 (비율 유지, 잘림)"), int(FitMode::Fill));
    m_fit->addItem(QStringLiteral("원본 1:1 (좌상단 기준)"), int(FitMode::Original));
    form->addRow(QStringLiteral("맞춤 방식"), m_fit);

    m_decode = new QComboBox(group);
    m_decode->addItem(QStringLiteral("전체 설정 따름 (캔버스 탭)"), int(DecodeMode::Default));
    m_decode->addItem(QStringLiteral("하드웨어 (그래픽카드)"), int(DecodeMode::Hardware));
    m_decode->addItem(QStringLiteral("CPU"), int(DecodeMode::Software));
    form->addRow(QStringLiteral("영상 디코딩"), m_decode);

    auto* row = new QHBoxLayout;
    m_opacity = new QSpinBox(group);
    m_opacity->setRange(0, 100);
    m_opacity->setSuffix(QStringLiteral(" %"));
    m_opacity->setKeyboardTracking(false);
    m_visible = new QCheckBox(QStringLiteral("표시"), group);
    row->addWidget(m_opacity);
    row->addWidget(m_visible);
    row->addStretch(1);
    form->addRow(QStringLiteral("불투명도"), row);
    layout->addWidget(group);

    // 콘텐츠 (장면별)
    auto* contentGroup = new QGroupBox(QStringLiteral("콘텐츠 (장면마다 다름)"), body);
    auto* contentLayout = new QVBoxLayout(contentGroup);
    m_content = new ContentEditor(controller, contentGroup);
    contentLayout->addWidget(m_content);
    layout->addWidget(contentGroup, 1);

    connect(m_addButton, &QPushButton::clicked, this, [this] { m_controller->addRegion(); });
    connect(m_removeButton, &QPushButton::clicked, this, [this] {
        if (!currentId().isEmpty())
            m_controller->removeRegion(currentId());
    });
    connect(m_upButton, &QPushButton::clicked, this, [this] { m_controller->moveRegion(currentId(), +1); });
    connect(m_downButton, &QPushButton::clicked, this, [this] { m_controller->moveRegion(currentId(), -1); });
    connect(m_list, &QListWidget::itemSelectionChanged, this, &RegionPanel::onListSelectionChanged);

    connect(m_name, &QLineEdit::editingFinished, this, &RegionPanel::commit);
    for (QSpinBox* box : {m_x, m_y, m_w, m_h, m_opacity})
        connect(box, QOverload<int>::of(&QSpinBox::valueChanged), this, &RegionPanel::commit);
    connect(m_fit, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RegionPanel::commit);
    connect(m_decode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RegionPanel::commit);
    connect(m_visible, &QCheckBox::toggled, this, &RegionPanel::commit);

    connect(controller, &ProjectController::projectReset, this, &RegionPanel::refreshList);
    connect(controller, &ProjectController::regionsChanged, this, &RegionPanel::refreshList);
    connect(controller, &ProjectController::regionSelected, this, &RegionPanel::refreshList);
    connect(controller, &ProjectController::editingSceneChanged, this, &RegionPanel::refreshList);
    connect(controller, &ProjectController::contentChanged, this, &RegionPanel::refreshList);

    refreshList();
}

QString RegionPanel::currentId() const
{
    return m_controller->selectedRegionId();
}

void RegionPanel::refreshList()
{
    const Project& project = m_controller->project();
    const QString selected = currentId();
    const QString sceneId = m_controller->editingSceneId();
    {
        const QSignalBlocker blocker(m_list);
        m_list->clear();
        // 배열의 마지막(맨 앞) 영역을 목록 맨 위에 표시
        for (auto it = project.regions.rbegin(); it != project.regions.rend(); ++it) {
            const QString id = QString::fromStdString(it->id);
            const RegionContent* content = m_controller->contentFor(sceneId, id);
            QString text = QStringLiteral("%1   (%2, %3  %4×%5)  · %6")
                               .arg(QString::fromStdString(it->name))
                               .arg(it->rect.x)
                               .arg(it->rect.y)
                               .arg(it->rect.w)
                               .arg(it->rect.h)
                               .arg(contentSummary(content));
            if (!it->visible)
                text += QStringLiteral("  [숨김]");
            auto* item = new QListWidgetItem(text, m_list);
            item->setData(Qt::UserRole, id);
            if (!content || content->isEmpty())
                item->setForeground(QColor(140, 140, 140));
            if (id == selected) {
                m_list->setCurrentItem(item);
                item->setSelected(true);
            }
        }
    }
    loadSelected();
}

void RegionPanel::onListSelectionChanged()
{
    const QList<QListWidgetItem*> items = m_list->selectedItems();
    if (items.isEmpty())
        return;
    m_controller->selectRegion(items.first()->data(Qt::UserRole).toString());
}

void RegionPanel::loadSelected()
{
    const Region* r = m_controller->project().findRegion(currentId().toStdString());
    const bool has = r != nullptr;
    m_form->setEnabled(has);
    m_removeButton->setEnabled(has);
    m_upButton->setEnabled(has);
    m_downButton->setEnabled(has);
    if (!has)
        return;

    m_loading = true;
    m_name->setText(QString::fromStdString(r->name));
    m_x->setValue(r->rect.x);
    m_y->setValue(r->rect.y);
    m_w->setValue(r->rect.w);
    m_h->setValue(r->rect.h);
    m_fit->setCurrentIndex(std::max(0, m_fit->findData(int(r->fit))));
    m_decode->setCurrentIndex(std::max(0, m_decode->findData(int(r->decode))));
    m_opacity->setValue(qRound(r->opacity * 100.0));
    m_visible->setChecked(r->visible);
    m_loading = false;
}

void RegionPanel::commit()
{
    if (m_loading)
        return;
    const Region* current = m_controller->project().findRegion(currentId().toStdString());
    if (!current)
        return;
    Region r = *current;
    r.name = m_name->text().trimmed().toStdString();
    if (r.name.empty())
        r.name = current->name;
    r.rect = RectI{m_x->value(), m_y->value(), m_w->value(), m_h->value()};
    r.fit = static_cast<FitMode>(m_fit->currentData().toInt());
    r.decode = static_cast<DecodeMode>(m_decode->currentData().toInt());
    r.opacity = m_opacity->value() / 100.0;
    r.visible = m_visible->isChecked();

    const bool changed = r.name != current->name || r.rect != current->rect || r.fit != current->fit ||
                         r.opacity != current->opacity || r.visible != current->visible ||
                         r.decode != current->decode;
    if (changed)
        m_controller->updateRegion(r);
}

} // namespace led
