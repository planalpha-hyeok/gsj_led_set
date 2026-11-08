#include "ui/PlaylistEditor.h"

#include "ui/LiveInputDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>

namespace led {

namespace {

const char kMediaFilter[] =
    "미디어 파일 (*.mp4 *.mov *.avi *.mkv *.wmv *.m4v *.webm *.mpg *.mpeg *.ts *.mxf *.gif "
    "*.png *.jpg *.jpeg *.bmp *.tif *.tiff *.webp);;"
    "영상 (*.mp4 *.mov *.avi *.mkv *.wmv *.m4v *.webm *.mpg *.mpeg *.ts *.mxf *.gif);;"
    "이미지 (*.png *.jpg *.jpeg *.bmp *.tif *.tiff *.webp);;"
    "모든 파일 (*.*)";

QString itemText(int index, const PlaylistItem& item)
{
    const bool live = item.type == ItemType::Live;
    QString file = live ? QStringLiteral("[라이브] %1").arg(QString::fromStdString(item.source))
                        : QFileInfo(QString::fromStdString(item.source)).fileName();
    if (live && !item.liveSize.empty())
        file += QStringLiteral(" %1").arg(QString::fromStdString(item.liveSize));
    const QString duration = item.duration > 0.0 ? QStringLiteral("%1초").arg(item.duration, 0, 'f', 1)
                             : live               ? QStringLiteral("계속")
                                                  : QStringLiteral("자동");
    const QString transition = item.transition == TransitionType::Fade
                                   ? QStringLiteral("페이드 %1초").arg(item.transitionDuration, 0, 'f', 1)
                                   : QStringLiteral("컷");
    return QStringLiteral("%1. %2   [%3 · %4]").arg(index + 1).arg(file, duration, transition);
}

} // namespace

PlaylistEditor::PlaylistEditor(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    m_list = new QListWidget(this);
    m_list->setMinimumHeight(110);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_list, 1);

    auto* buttons = new QHBoxLayout;
    m_addButton = new QPushButton(QStringLiteral("파일 추가..."), this);
    m_addLiveButton = new QPushButton(QStringLiteral("라이브 입력 추가..."), this);
    m_addLiveButton->setToolTip(QStringLiteral("캡처보드/웹캠 영상을 이 영역에 표시"));
    m_removeButton = new QPushButton(QStringLiteral("삭제"), this);
    m_upButton = new QPushButton(QStringLiteral("위로"), this);
    m_downButton = new QPushButton(QStringLiteral("아래로"), this);
    buttons->addWidget(m_addButton);
    buttons->addWidget(m_addLiveButton);
    buttons->addWidget(m_removeButton);
    buttons->addWidget(m_upButton);
    buttons->addWidget(m_downButton);
    layout->addLayout(buttons);

    m_itemForm = new QWidget(this);
    auto* form = new QFormLayout(m_itemForm);
    form->setContentsMargins(0, 0, 0, 0);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_duration = new QDoubleSpinBox(m_itemForm);
    m_duration->setRange(0.0, 86400.0);
    m_duration->setDecimals(1);
    m_duration->setSingleStep(1.0);
    m_duration->setSuffix(QStringLiteral(" 초"));
    m_duration->setSpecialValueText(QStringLiteral("자동 (영상: 끝까지, 이미지: 10초, 라이브: 계속)"));
    m_duration->setKeyboardTracking(false);
    form->addRow(QStringLiteral("재생 시간"), m_duration);

    auto* transitionRow = new QHBoxLayout;
    m_transition = new QComboBox(m_itemForm);
    m_transition->addItem(QStringLiteral("컷 (즉시 전환)"), int(TransitionType::Cut));
    m_transition->addItem(QStringLiteral("페이드"), int(TransitionType::Fade));
    m_fade = new QDoubleSpinBox(m_itemForm);
    m_fade->setRange(0.1, 30.0);
    m_fade->setDecimals(1);
    m_fade->setSingleStep(0.5);
    m_fade->setSuffix(QStringLiteral(" 초"));
    m_fade->setKeyboardTracking(false);
    transitionRow->addWidget(m_transition, 1);
    transitionRow->addWidget(m_fade);
    form->addRow(QStringLiteral("시작 전환"), transitionRow);

    m_liveSettingsButton = new QPushButton(QStringLiteral("라이브 입력 설정..."), m_itemForm);
    form->addRow(QString(), m_liveSettingsButton);
    layout->addWidget(m_itemForm);

    m_loop = new QCheckBox(QStringLiteral("목록 반복 (끄면 마지막 항목 화면 유지)"), this);
    layout->addWidget(m_loop);

    auto* hint = new QLabel(QStringLiteral("전환은 해당 항목이 시작될 때 적용됩니다."), this);
    hint->setObjectName("hintLabel");
    layout->addWidget(hint);

    connect(m_addButton, &QPushButton::clicked, this, &PlaylistEditor::addItems);
    connect(m_addLiveButton, &QPushButton::clicked, this, &PlaylistEditor::addLiveInput);
    connect(m_liveSettingsButton, &QPushButton::clicked, this, &PlaylistEditor::editLiveInput);
    connect(m_removeButton, &QPushButton::clicked, this, &PlaylistEditor::removeItem);
    connect(m_upButton, &QPushButton::clicked, this, [this] { moveItem(-1); });
    connect(m_downButton, &QPushButton::clicked, this, [this] { moveItem(+1); });
    connect(m_list, &QListWidget::itemSelectionChanged, this, &PlaylistEditor::onSelectionChanged);
    connect(m_duration, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PlaylistEditor::commitItem);
    connect(m_transition, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PlaylistEditor::commitItem);
    connect(m_fade, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PlaylistEditor::commitItem);
    connect(m_loop, &QCheckBox::toggled, this, &PlaylistEditor::commitLoop);

    refreshList({});
}

int PlaylistEditor::currentIndex() const
{
    const int row = m_list->currentRow();
    return row >= 0 && row < static_cast<int>(m_content.items.size()) ? row : -1;
}

void PlaylistEditor::setContent(const RegionContent& content)
{
    const int index = currentIndex();
    const std::string selectId = index >= 0 ? m_content.items[static_cast<std::size_t>(index)].id : std::string();
    m_content = content;
    m_loading = true;
    m_loop->setChecked(content.loopPlaylist);
    m_loading = false;
    refreshList(selectId);
}

void PlaylistEditor::refreshList(const std::string& selectId)
{
    {
        const QSignalBlocker blocker(m_list);
        m_list->clear();
        int selectRow = m_content.items.empty() ? -1 : 0;
        for (int i = 0; i < static_cast<int>(m_content.items.size()); ++i) {
            const PlaylistItem& item = m_content.items[static_cast<std::size_t>(i)];
            auto* row = new QListWidgetItem(itemText(i, item), m_list);
            row->setToolTip(QString::fromStdString(item.source));
            if (!selectId.empty() && item.id == selectId)
                selectRow = i;
        }
        if (selectRow >= 0)
            m_list->setCurrentRow(selectRow);
    }
    loadItem();
}

void PlaylistEditor::loadItem()
{
    const int index = currentIndex();
    const bool has = index >= 0;
    m_itemForm->setEnabled(has);
    m_removeButton->setEnabled(has);
    m_upButton->setEnabled(has && index > 0);
    m_downButton->setEnabled(has && index + 1 < static_cast<int>(m_content.items.size()));
    if (!has)
        return;
    const PlaylistItem& item = m_content.items[static_cast<std::size_t>(index)];
    m_loading = true;
    m_duration->setValue(item.duration);
    m_transition->setCurrentIndex(std::max(0, m_transition->findData(int(item.transition))));
    m_fade->setValue(std::max(0.1, item.transitionDuration));
    m_fade->setEnabled(item.transition == TransitionType::Fade);
    m_liveSettingsButton->setVisible(item.type == ItemType::Live);
    m_loading = false;
}

void PlaylistEditor::onSelectionChanged()
{
    loadItem();
}

void PlaylistEditor::addItems()
{
    QSettings settings;
    const QString start = settings.value(QStringLiteral("lastMediaFolder")).toString();
    const QStringList files = QFileDialog::getOpenFileNames(this, QStringLiteral("콘텐츠 추가"), start,
                                                            QString::fromUtf8(kMediaFilter));
    if (files.isEmpty())
        return;
    settings.setValue(QStringLiteral("lastMediaFolder"), QFileInfo(files.first()).path());

    std::string lastId;
    for (const QString& file : files) {
        PlaylistItem item;
        item.id = generateId();
        item.source = file.toStdString();
        lastId = item.id;
        m_content.items.push_back(std::move(item));
    }
    refreshList(lastId);
    emit edited(m_content);
}

void PlaylistEditor::addLiveInput()
{
    LiveInputDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    PlaylistItem item = dialog.item();
    item.id = generateId();
    m_content.items.push_back(item);
    refreshList(item.id);
    emit edited(m_content);
}

void PlaylistEditor::editLiveInput()
{
    const int index = currentIndex();
    if (index < 0 || m_content.items[static_cast<std::size_t>(index)].type != ItemType::Live)
        return;
    LiveInputDialog dialog(this);
    dialog.setItem(m_content.items[static_cast<std::size_t>(index)]);
    if (dialog.exec() != QDialog::Accepted)
        return;
    PlaylistItem updated = dialog.item();
    PlaylistItem& item = m_content.items[static_cast<std::size_t>(index)];
    item.source = updated.source;
    item.liveSize = updated.liveSize;
    item.liveFps = updated.liveFps;
    refreshList(item.id);
    emit edited(m_content);
}

void PlaylistEditor::removeItem()
{
    const int index = currentIndex();
    if (index < 0)
        return;
    m_content.items.erase(m_content.items.begin() + index);
    std::string selectId;
    if (!m_content.items.empty())
        selectId = m_content.items[static_cast<std::size_t>(std::min(index, int(m_content.items.size()) - 1))].id;
    refreshList(selectId);
    emit edited(m_content);
}

void PlaylistEditor::moveItem(int delta)
{
    const int index = currentIndex();
    const int target = index + delta;
    if (index < 0 || target < 0 || target >= static_cast<int>(m_content.items.size()))
        return;
    std::swap(m_content.items[static_cast<std::size_t>(index)], m_content.items[static_cast<std::size_t>(target)]);
    refreshList(m_content.items[static_cast<std::size_t>(target)].id);
    emit edited(m_content);
}

void PlaylistEditor::commitItem()
{
    if (m_loading)
        return;
    const int index = currentIndex();
    if (index < 0)
        return;
    PlaylistItem& item = m_content.items[static_cast<std::size_t>(index)];
    const PlaylistItem before = item;
    item.duration = m_duration->value();
    item.transition = static_cast<TransitionType>(m_transition->currentData().toInt());
    item.transitionDuration = m_fade->value();
    m_fade->setEnabled(item.transition == TransitionType::Fade);
    if (item == before)
        return;
    m_list->item(index)->setText(itemText(index, item));
    emit edited(m_content);
}

void PlaylistEditor::commitLoop()
{
    if (m_loading || m_content.loopPlaylist == m_loop->isChecked())
        return;
    m_content.loopPlaylist = m_loop->isChecked();
    emit edited(m_content);
}

} // namespace led
