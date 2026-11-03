#include "ui/RegionPanel.h"

#include "core/Project.h"
#include "ui/FormHelpers.h"
#include "ui/ProjectController.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
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

#include <algorithm>

namespace led {

namespace {

constexpr int kCoordLimit = 1000000;

const char kMediaFilter[] =
    "미디어 파일 (*.mp4 *.mov *.avi *.mkv *.wmv *.m4v *.webm *.mpg *.mpeg *.ts *.mxf *.gif "
    "*.png *.jpg *.jpeg *.bmp *.tif *.tiff *.webp);;"
    "영상 (*.mp4 *.mov *.avi *.mkv *.wmv *.m4v *.webm *.mpg *.mpeg *.ts *.mxf *.gif);;"
    "이미지 (*.png *.jpg *.jpeg *.bmp *.tif *.tiff *.webp);;"
    "모든 파일 (*.*)";

} // namespace

RegionPanel::RegionPanel(ProjectController* controller, QWidget* parent)
    : QWidget(parent)
    , m_controller(controller)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // 목록 (위쪽이 화면 앞쪽)
    auto* listLabel = new QLabel(QStringLiteral("영역 목록 (위쪽이 화면 앞)"), this);
    listLabel->setObjectName("sectionLabel");
    layout->addWidget(listLabel);

    m_list = new QListWidget(this);
    m_list->setMinimumHeight(120);
    layout->addWidget(m_list, 1);

    auto* buttons = new QHBoxLayout;
    m_addButton = new QPushButton(QStringLiteral("추가"), this);
    m_removeButton = new QPushButton(QStringLiteral("삭제"), this);
    m_upButton = new QPushButton(QStringLiteral("앞으로"), this);
    m_downButton = new QPushButton(QStringLiteral("뒤로"), this);
    for (QPushButton* b : {m_addButton, m_removeButton, m_upButton, m_downButton})
        buttons->addWidget(b);
    layout->addLayout(buttons);

    // 속성
    m_form = new QGroupBox(QStringLiteral("영역 속성"), this);
    auto* form = new QFormLayout(m_form);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_name = new QLineEdit(m_form);
    form->addRow(QStringLiteral("이름"), m_name);

    auto* geometry = new QGridLayout;
    m_x = makePixelSpinBox(-kCoordLimit, kCoordLimit, m_form);
    m_y = makePixelSpinBox(-kCoordLimit, kCoordLimit, m_form);
    m_w = makePixelSpinBox(1, kCoordLimit, m_form);
    m_h = makePixelSpinBox(1, kCoordLimit, m_form);
    geometry->addWidget(new QLabel(QStringLiteral("X"), m_form), 0, 0);
    geometry->addWidget(m_x, 0, 1);
    geometry->addWidget(new QLabel(QStringLiteral("Y"), m_form), 0, 2);
    geometry->addWidget(m_y, 0, 3);
    geometry->addWidget(new QLabel(QStringLiteral("너비"), m_form), 1, 0);
    geometry->addWidget(m_w, 1, 1);
    geometry->addWidget(new QLabel(QStringLiteral("높이"), m_form), 1, 2);
    geometry->addWidget(m_h, 1, 3);
    form->addRow(QStringLiteral("위치/크기"), geometry);

    auto* sourceRow = new QHBoxLayout;
    m_source = new QLineEdit(m_form);
    m_source->setReadOnly(true);
    m_source->setPlaceholderText(QStringLiteral("콘텐츠 없음"));
    m_browseButton = new QPushButton(QStringLiteral("찾기..."), m_form);
    m_clearButton = new QPushButton(QStringLiteral("비우기"), m_form);
    sourceRow->addWidget(m_source, 1);
    sourceRow->addWidget(m_browseButton);
    sourceRow->addWidget(m_clearButton);
    form->addRow(QStringLiteral("콘텐츠"), sourceRow);

    m_fit = new QComboBox(m_form);
    m_fit->addItem(QStringLiteral("늘리기 (영역에 꽉 맞춤)"), int(FitMode::Stretch));
    m_fit->addItem(QStringLiteral("맞추기 (비율 유지, 여백)"), int(FitMode::Fit));
    m_fit->addItem(QStringLiteral("채우기 (비율 유지, 잘림)"), int(FitMode::Fill));
    m_fit->addItem(QStringLiteral("원본 1:1 (좌상단 기준)"), int(FitMode::Original));
    form->addRow(QStringLiteral("맞춤 방식"), m_fit);

    m_opacity = new QSpinBox(m_form);
    m_opacity->setRange(0, 100);
    m_opacity->setSuffix(QStringLiteral(" %"));
    m_opacity->setKeyboardTracking(false);
    form->addRow(QStringLiteral("불투명도"), m_opacity);

    m_loop = new QCheckBox(QStringLiteral("반복 재생"), m_form);
    m_visible = new QCheckBox(QStringLiteral("표시"), m_form);
    auto* checks = new QHBoxLayout;
    checks->addWidget(m_loop);
    checks->addWidget(m_visible);
    checks->addStretch(1);
    form->addRow(QString(), checks);

    layout->addWidget(m_form);

    // 연결
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
    connect(m_loop, &QCheckBox::toggled, this, &RegionPanel::commit);
    connect(m_visible, &QCheckBox::toggled, this, &RegionPanel::commit);
    connect(m_browseButton, &QPushButton::clicked, this, &RegionPanel::browseSource);
    connect(m_clearButton, &QPushButton::clicked, this, &RegionPanel::clearSource);

    connect(controller, &ProjectController::projectReset, this, &RegionPanel::refreshList);
    connect(controller, &ProjectController::regionsChanged, this, &RegionPanel::refreshList);
    connect(controller, &ProjectController::regionSelected, this, &RegionPanel::refreshList);

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
    {
        const QSignalBlocker blocker(m_list);
        m_list->clear();
        // 배열의 마지막(맨 앞) 영역을 목록 맨 위에 표시
        for (auto it = project.regions.rbegin(); it != project.regions.rend(); ++it) {
            QString text = QStringLiteral("%1   (%2, %3  %4×%5)")
                               .arg(QString::fromStdString(it->name))
                               .arg(it->rect.x)
                               .arg(it->rect.y)
                               .arg(it->rect.w)
                               .arg(it->rect.h);
            if (!it->visible)
                text += QStringLiteral("  [숨김]");
            auto* item = new QListWidgetItem(text, m_list);
            item->setData(Qt::UserRole, QString::fromStdString(it->id));
            if (it->source.empty())
                item->setForeground(QColor(140, 140, 140));
            if (QString::fromStdString(it->id) == selected) {
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
    const QString source = QString::fromStdString(r->source);
    m_source->setText(source.isEmpty() ? QString() : QFileInfo(source).fileName());
    m_source->setToolTip(source);
    m_fit->setCurrentIndex(std::max(0, m_fit->findData(int(r->fit))));
    m_opacity->setValue(qRound(r->opacity * 100.0));
    m_loop->setChecked(r->loop);
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
    r.opacity = m_opacity->value() / 100.0;
    r.loop = m_loop->isChecked();
    r.visible = m_visible->isChecked();

    const bool changed = r.name != current->name || r.rect != current->rect || r.fit != current->fit ||
                         r.opacity != current->opacity || r.loop != current->loop || r.visible != current->visible;
    if (changed)
        m_controller->updateRegion(r);
}

void RegionPanel::browseSource()
{
    const Region* current = m_controller->project().findRegion(currentId().toStdString());
    if (!current)
        return;
    const QString start = current->source.empty() ? QString() : QFileInfo(QString::fromStdString(current->source)).path();
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("콘텐츠 선택"), start,
                                                      QString::fromUtf8(kMediaFilter));
    if (path.isEmpty())
        return;
    Region r = *current;
    r.source = path.toStdString();
    m_controller->updateRegion(r);
}

void RegionPanel::clearSource()
{
    const Region* current = m_controller->project().findRegion(currentId().toStdString());
    if (!current || current->source.empty())
        return;
    Region r = *current;
    r.source.clear();
    m_controller->updateRegion(r);
}

} // namespace led
