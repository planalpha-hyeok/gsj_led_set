#include "ui/SchedulePanel.h"

#include "core/Project.h"
#include "ui/ProjectController.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QTime>
#include <QTimeEdit>
#include <QVBoxLayout>

#include <algorithm>

namespace led {

namespace {

const char* const kDayNames[7] = {"월", "화", "수", "목", "금", "토", "일"};

QString daysText(std::uint8_t mask)
{
    if ((mask & 0x7F) == 0x7F)
        return QStringLiteral("매일");
    if ((mask & 0x7F) == 0x1F)
        return QStringLiteral("평일");
    if ((mask & 0x7F) == 0x60)
        return QStringLiteral("주말");
    QString text;
    for (int i = 0; i < 7; ++i)
        if (mask & (1u << i))
            text += QString::fromUtf8(kDayNames[i]);
    return text.isEmpty() ? QStringLiteral("(요일 없음)") : text;
}

QString entryText(const ScheduleEntry& e, const Project& project)
{
    const Scene* scene = project.findScene(e.sceneId);
    const QString when = e.date.empty() ? daysText(e.weekdays) : QString::fromStdString(e.date);
    QString time;
    if (e.startMinute == e.endMinute)
        time = QStringLiteral("하루 종일");
    else
        time = QStringLiteral("%1~%2%3")
                   .arg(QString::fromStdString(minuteToString(e.startMinute)),
                        QString::fromStdString(minuteToString(e.endMinute)),
                        e.endMinute < e.startMinute ? QStringLiteral("(다음날)") : QString());
    return QStringLiteral("%1 %2 · %3 %4 → %5")
        .arg(e.enabled ? QStringLiteral("✔") : QStringLiteral("✖"))
        .arg(QString::fromStdString(e.name), when, time,
             scene ? QString::fromStdString(scene->name) : QStringLiteral("(장면 없음)"));
}

} // namespace

SchedulePanel::SchedulePanel(ProjectController* controller, QWidget* parent)
    : QWidget(parent)
    , m_controller(controller)
{
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

    // 운영 설정
    auto* settings = new QGroupBox(QStringLiteral("운영"), body);
    auto* sform = new QFormLayout(settings);
    sform->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_enabled = new QCheckBox(QStringLiteral("자동 실행 시 스케줄로 운영 (끄면 첫 번째 장면 재생)"), settings);
    sform->addRow(QString(), m_enabled);

    auto* outsideRow = new QHBoxLayout;
    m_outside = new QComboBox(settings);
    m_outside->addItem(QStringLiteral("정지 (검은 화면)"), int(OutsideBehavior::Black));
    m_outside->addItem(QStringLiteral("기본 장면 재생"), int(OutsideBehavior::DefaultScene));
    m_defaultScene = new QComboBox(settings);
    outsideRow->addWidget(m_outside);
    outsideRow->addWidget(m_defaultScene, 1);
    sform->addRow(QStringLiteral("일정 없는 시간"), outsideRow);

    m_autoStart = new QCheckBox(QStringLiteral("프로그램 시작 시 자동 실행 (출력 켜기 + 재생 또는 스케줄 운영)"),
                                settings);
    sform->addRow(QString(), m_autoStart);

    m_status = new QLabel(settings);
    m_status->setObjectName("hintLabel");
    m_status->setWordWrap(true);
    sform->addRow(QStringLiteral("현재 상태"), m_status);
    layout->addWidget(settings);

    // 일정 목록
    auto* listLabel = new QLabel(QStringLiteral("일정 목록 (날짜 지정 일정 > 요일 일정, 같은 종류는 위쪽 우선)"), body);
    listLabel->setObjectName("sectionLabel");
    listLabel->setWordWrap(true);
    layout->addWidget(listLabel);

    m_list = new QListWidget(body);
    m_list->setMinimumHeight(110);
    m_list->setMaximumHeight(200);
    layout->addWidget(m_list);

    auto* buttons = new QHBoxLayout;
    m_addButton = new QPushButton(QStringLiteral("추가"), body);
    m_removeButton = new QPushButton(QStringLiteral("삭제"), body);
    m_upButton = new QPushButton(QStringLiteral("위로"), body);
    m_downButton = new QPushButton(QStringLiteral("아래로"), body);
    for (QPushButton* b : {m_addButton, m_removeButton, m_upButton, m_downButton})
        buttons->addWidget(b);
    layout->addLayout(buttons);

    // 일정 편집
    auto* entryGroup = new QGroupBox(QStringLiteral("일정"), body);
    m_entryForm = entryGroup;
    auto* eform = new QFormLayout(entryGroup);
    eform->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto* nameRow = new QHBoxLayout;
    m_name = new QLineEdit(entryGroup);
    m_entryEnabled = new QCheckBox(QStringLiteral("사용"), entryGroup);
    nameRow->addWidget(m_name, 1);
    nameRow->addWidget(m_entryEnabled);
    eform->addRow(QStringLiteral("이름"), nameRow);

    m_scene = new QComboBox(entryGroup);
    eform->addRow(QStringLiteral("장면"), m_scene);

    auto* modeRow = new QHBoxLayout;
    m_weekdayMode = new QRadioButton(QStringLiteral("요일 반복"), entryGroup);
    m_dateMode = new QRadioButton(QStringLiteral("특정 날짜"), entryGroup);
    auto* modeGroup = new QButtonGroup(entryGroup);
    modeGroup->addButton(m_weekdayMode);
    modeGroup->addButton(m_dateMode);
    modeRow->addWidget(m_weekdayMode);
    modeRow->addWidget(m_dateMode);
    modeRow->addStretch(1);
    eform->addRow(QStringLiteral("적용"), modeRow);

    auto* daysRow = new QHBoxLayout;
    for (int i = 0; i < 7; ++i) {
        m_days[i] = new QCheckBox(QString::fromUtf8(kDayNames[i]), entryGroup);
        daysRow->addWidget(m_days[i]);
    }
    daysRow->addStretch(1);
    eform->addRow(QStringLiteral("요일"), daysRow);

    m_date = new QDateEdit(entryGroup);
    m_date->setCalendarPopup(true);
    m_date->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    eform->addRow(QStringLiteral("날짜"), m_date);

    auto* timeRow = new QHBoxLayout;
    m_start = new QTimeEdit(entryGroup);
    m_end = new QTimeEdit(entryGroup);
    for (QTimeEdit* t : {m_start, m_end}) {
        t->setDisplayFormat(QStringLiteral("HH:mm"));
        t->setKeyboardTracking(false);
    }
    timeRow->addWidget(m_start);
    timeRow->addWidget(new QLabel(QStringLiteral("~"), entryGroup));
    timeRow->addWidget(m_end);
    timeRow->addStretch(1);
    eform->addRow(QStringLiteral("시각"), timeRow);

    m_entryHint = new QLabel(entryGroup);
    m_entryHint->setObjectName("hintLabel");
    m_entryHint->setWordWrap(true);
    eform->addRow(QString(), m_entryHint);
    layout->addWidget(entryGroup);

    auto* hint = new QLabel(QStringLiteral("종료 시각이 시작보다 이르면 다음날 종료 시각까지 적용됩니다 (예: 22:00~02:00).\n"
                                           "시작과 종료가 같으면 하루 종일 적용됩니다."),
                            body);
    hint->setObjectName("hintLabel");
    hint->setWordWrap(true);
    layout->addWidget(hint);
    layout->addStretch(1);

    // 연결
    connect(m_enabled, &QCheckBox::toggled, this, &SchedulePanel::commitSettings);
    connect(m_outside, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SchedulePanel::commitSettings);
    connect(m_defaultScene, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &SchedulePanel::commitSettings);
    connect(m_autoStart, &QCheckBox::toggled, this, [this](bool on) {
        if (!m_loading)
            m_controller->setAutoStart(on);
    });

    connect(m_list, &QListWidget::itemSelectionChanged, this, [this] {
        const auto items = m_list->selectedItems();
        if (items.isEmpty())
            return;
        m_selectedEntry = items.first()->data(Qt::UserRole).toString();
        loadEntry();
    });
    connect(m_addButton, &QPushButton::clicked, this, &SchedulePanel::addEntry);
    connect(m_removeButton, &QPushButton::clicked, this, &SchedulePanel::removeEntry);
    connect(m_upButton, &QPushButton::clicked, this, [this] { moveEntry(-1); });
    connect(m_downButton, &QPushButton::clicked, this, [this] { moveEntry(+1); });

    connect(m_name, &QLineEdit::editingFinished, this, &SchedulePanel::commitEntry);
    connect(m_entryEnabled, &QCheckBox::toggled, this, &SchedulePanel::commitEntry);
    connect(m_scene, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SchedulePanel::commitEntry);
    connect(m_weekdayMode, &QRadioButton::toggled, this, &SchedulePanel::commitEntry);
    for (QCheckBox* day : m_days)
        connect(day, &QCheckBox::toggled, this, &SchedulePanel::commitEntry);
    connect(m_date, &QDateEdit::dateChanged, this, &SchedulePanel::commitEntry);
    connect(m_start, &QTimeEdit::timeChanged, this, &SchedulePanel::commitEntry);
    connect(m_end, &QTimeEdit::timeChanged, this, &SchedulePanel::commitEntry);

    connect(controller, &ProjectController::projectReset, this, [this] {
        m_selectedEntry.clear();
        refresh();
    });
    connect(controller, &ProjectController::scheduleChanged, this, &SchedulePanel::refresh);
    connect(controller, &ProjectController::scenesChanged, this, &SchedulePanel::refresh);
    refresh();
}

void SchedulePanel::setStatusText(const QString& text)
{
    m_status->setText(text.isEmpty() ? QStringLiteral("스케줄 운영 꺼짐") : text);
}

int SchedulePanel::currentIndex() const
{
    const auto& entries = m_controller->project().schedule.entries;
    for (int i = 0; i < static_cast<int>(entries.size()); ++i)
        if (QString::fromStdString(entries[static_cast<std::size_t>(i)].id) == m_selectedEntry)
            return i;
    return -1;
}

void SchedulePanel::fillSceneCombo(QComboBox* combo, const std::string& selectedId)
{
    const QSignalBlocker blocker(combo);
    combo->clear();
    int selected = -1;
    for (const Scene& s : m_controller->project().scenes) {
        combo->addItem(QString::fromStdString(s.name), QString::fromStdString(s.id));
        if (s.id == selectedId)
            selected = combo->count() - 1;
    }
    if (selected < 0 && !selectedId.empty()) {
        combo->addItem(QStringLiteral("(삭제된 장면)"), QString::fromStdString(selectedId));
        selected = combo->count() - 1;
    }
    combo->setCurrentIndex(selected);
}

void SchedulePanel::refresh()
{
    const Project& project = m_controller->project();
    const Schedule& schedule = project.schedule;

    m_loading = true;
    m_enabled->setChecked(schedule.enabled);
    m_outside->setCurrentIndex(schedule.outside == OutsideBehavior::DefaultScene ? 1 : 0);
    fillSceneCombo(m_defaultScene, schedule.defaultSceneId);
    m_defaultScene->setEnabled(schedule.outside == OutsideBehavior::DefaultScene);
    m_autoStart->setChecked(project.autoStart);
    m_loading = false;

    if (currentIndex() < 0)
        m_selectedEntry = schedule.entries.empty() ? QString() : QString::fromStdString(schedule.entries.front().id);

    {
        const QSignalBlocker blocker(m_list);
        m_list->clear();
        for (const ScheduleEntry& e : schedule.entries) {
            auto* item = new QListWidgetItem(entryText(e, project), m_list);
            const QString id = QString::fromStdString(e.id);
            item->setData(Qt::UserRole, id);
            if (!e.enabled || !project.findScene(e.sceneId))
                item->setForeground(QColor(150, 150, 150));
            if (id == m_selectedEntry) {
                m_list->setCurrentItem(item);
                item->setSelected(true);
            }
        }
    }
    loadEntry();
}

void SchedulePanel::loadEntry()
{
    const int index = currentIndex();
    const bool has = index >= 0;
    m_entryForm->setEnabled(has);
    m_removeButton->setEnabled(has);
    m_upButton->setEnabled(has && index > 0);
    m_downButton->setEnabled(has && index + 1 < static_cast<int>(m_controller->project().schedule.entries.size()));
    if (!has) {
        m_entryHint->clear();
        return;
    }
    const ScheduleEntry& e = m_controller->project().schedule.entries[static_cast<std::size_t>(index)];

    m_loading = true;
    m_name->setText(QString::fromStdString(e.name));
    m_entryEnabled->setChecked(e.enabled);
    fillSceneCombo(m_scene, e.sceneId);
    const bool dateMode = !e.date.empty();
    m_dateMode->setChecked(dateMode);
    m_weekdayMode->setChecked(!dateMode);
    for (int i = 0; i < 7; ++i) {
        m_days[i]->setChecked((e.weekdays & (1u << i)) != 0);
        m_days[i]->setEnabled(!dateMode);
    }
    const QDate date = QDate::fromString(QString::fromStdString(e.date), QStringLiteral("yyyy-MM-dd"));
    m_date->setDate(date.isValid() ? date : QDate::currentDate());
    m_date->setEnabled(dateMode);
    m_start->setTime(QTime(e.startMinute / 60, e.startMinute % 60));
    m_end->setTime(QTime(e.endMinute / 60, e.endMinute % 60));
    m_loading = false;

    QString hint;
    if (!dateMode && (e.weekdays & 0x7F) == 0)
        hint = QStringLiteral("요일이 선택되지 않아 이 일정은 적용되지 않습니다.");
    else if (!m_controller->project().findScene(e.sceneId))
        hint = QStringLiteral("장면이 지정되지 않아 이 일정은 적용되지 않습니다.");
    m_entryHint->setText(hint);
}

void SchedulePanel::commitSettings()
{
    if (m_loading)
        return;
    Schedule s = m_controller->project().schedule;
    const bool enabled = m_enabled->isChecked();
    s.outside = static_cast<OutsideBehavior>(m_outside->currentData().toInt());
    s.defaultSceneId = m_defaultScene->currentData().toString().toStdString();
    const Schedule& current = m_controller->project().schedule;
    if (s.outside != current.outside || s.defaultSceneId != current.defaultSceneId) {
        s.enabled = current.enabled;
        m_controller->setSchedule(s);
    }
    m_controller->setScheduleEnabled(enabled);
}

void SchedulePanel::commitEntry()
{
    if (m_loading)
        return;
    const int index = currentIndex();
    if (index < 0)
        return;
    Schedule s = m_controller->project().schedule;
    ScheduleEntry& e = s.entries[static_cast<std::size_t>(index)];
    const ScheduleEntry before = e;

    const std::string name = m_name->text().trimmed().toStdString();
    if (!name.empty())
        e.name = name;
    e.enabled = m_entryEnabled->isChecked();
    e.sceneId = m_scene->currentData().toString().toStdString();
    std::uint8_t mask = 0;
    for (int i = 0; i < 7; ++i)
        if (m_days[i]->isChecked())
            mask |= static_cast<std::uint8_t>(1u << i);
    e.weekdays = mask;
    e.date = m_dateMode->isChecked() ? m_date->date().toString(QStringLiteral("yyyy-MM-dd")).toStdString()
                                     : std::string();
    e.startMinute = m_start->time().hour() * 60 + m_start->time().minute();
    e.endMinute = m_end->time().hour() * 60 + m_end->time().minute();

    const bool changed = e.name != before.name || e.enabled != before.enabled || e.sceneId != before.sceneId ||
                         e.weekdays != before.weekdays || e.date != before.date ||
                         e.startMinute != before.startMinute || e.endMinute != before.endMinute;
    if (changed)
        m_controller->setSchedule(s);
}

void SchedulePanel::addEntry()
{
    Schedule s = m_controller->project().schedule;
    ScheduleEntry e;
    e.id = generateId();
    e.name = "일정 " + std::to_string(s.entries.size() + 1);
    e.sceneId = m_controller->editingSceneId().toStdString();
    s.entries.push_back(e);
    m_selectedEntry = QString::fromStdString(e.id);
    m_controller->setSchedule(s);
}

void SchedulePanel::removeEntry()
{
    const int index = currentIndex();
    if (index < 0)
        return;
    Schedule s = m_controller->project().schedule;
    s.entries.erase(s.entries.begin() + index);
    m_selectedEntry.clear();
    if (!s.entries.empty())
        m_selectedEntry = QString::fromStdString(
            s.entries[static_cast<std::size_t>(std::min(index, int(s.entries.size()) - 1))].id);
    m_controller->setSchedule(s);
}

void SchedulePanel::moveEntry(int delta)
{
    const int index = currentIndex();
    Schedule s = m_controller->project().schedule;
    const int target = index + delta;
    if (index < 0 || target < 0 || target >= static_cast<int>(s.entries.size()))
        return;
    std::swap(s.entries[static_cast<std::size_t>(index)], s.entries[static_cast<std::size_t>(target)]);
    m_controller->setSchedule(s);
}

} // namespace led
