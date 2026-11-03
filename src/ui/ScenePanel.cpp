#include "ui/ScenePanel.h"

#include "core/Project.h"
#include "ui/ProjectController.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace led {

ScenePanel::ScenePanel(ProjectController* controller, QWidget* parent)
    : QWidget(parent)
    , m_controller(controller)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto* label = new QLabel(QStringLiteral("장면 목록 (선택한 장면의 콘텐츠를 '영역' 탭에서 편집)"), this);
    label->setObjectName("sectionLabel");
    label->setWordWrap(true);
    layout->addWidget(label);

    m_list = new QListWidget(this);
    layout->addWidget(m_list, 1);

    auto* buttons = new QHBoxLayout;
    m_addButton = new QPushButton(QStringLiteral("추가"), this);
    m_duplicateButton = new QPushButton(QStringLiteral("복제"), this);
    m_removeButton = new QPushButton(QStringLiteral("삭제"), this);
    m_upButton = new QPushButton(QStringLiteral("위로"), this);
    m_downButton = new QPushButton(QStringLiteral("아래로"), this);
    for (QPushButton* b : {m_addButton, m_duplicateButton, m_removeButton, m_upButton, m_downButton})
        buttons->addWidget(b);
    layout->addLayout(buttons);

    auto* form = new QFormLayout;
    m_name = new QLineEdit(this);
    form->addRow(QStringLiteral("이름"), m_name);
    layout->addLayout(form);

    m_playButton = new QPushButton(QStringLiteral("▶ 이 장면 재생"), this);
    layout->addWidget(m_playButton);

    auto* hint = new QLabel(QStringLiteral("영역의 위치는 모든 장면이 같고, 영역에 넣는 콘텐츠만 장면마다 다릅니다.\n"
                                           "장면은 '스케줄' 탭에서 시간대별로 자동 전환할 수 있습니다."),
                            this);
    hint->setObjectName("hintLabel");
    hint->setWordWrap(true);
    layout->addWidget(hint);

    connect(m_list, &QListWidget::itemSelectionChanged, this, [this] {
        const QString id = selectedId();
        if (!id.isEmpty())
            m_controller->setEditingScene(id);
    });
    connect(m_addButton, &QPushButton::clicked, this, [this] { m_controller->addScene(); });
    connect(m_duplicateButton, &QPushButton::clicked, this,
            [this] { m_controller->duplicateScene(m_controller->editingSceneId()); });
    connect(m_removeButton, &QPushButton::clicked, this, [this] {
        const Scene* scene = m_controller->editingScene();
        if (!scene)
            return;
        const auto answer = QMessageBox::question(
            this, QStringLiteral("장면 삭제"),
            QStringLiteral("장면 '%1'을(를) 삭제하시겠습니까?\n이 장면을 쓰는 스케줄 일정은 동작하지 않게 됩니다.")
                .arg(QString::fromStdString(scene->name)));
        if (answer != QMessageBox::Yes)
            return;
        if (!m_controller->removeScene(m_controller->editingSceneId()))
            QMessageBox::information(this, QStringLiteral("장면 삭제"),
                                     QStringLiteral("장면은 최소 1개가 있어야 합니다."));
    });
    connect(m_upButton, &QPushButton::clicked, this, [this] { m_controller->moveScene(m_controller->editingSceneId(), -1); });
    connect(m_downButton, &QPushButton::clicked, this,
            [this] { m_controller->moveScene(m_controller->editingSceneId(), +1); });
    connect(m_name, &QLineEdit::editingFinished, this,
            [this] { m_controller->renameScene(m_controller->editingSceneId(), m_name->text()); });
    connect(m_playButton, &QPushButton::clicked, this,
            [this] { emit playSceneRequested(m_controller->editingSceneId()); });

    connect(controller, &ProjectController::projectReset, this, &ScenePanel::refresh);
    connect(controller, &ProjectController::scenesChanged, this, &ScenePanel::refresh);
    connect(controller, &ProjectController::editingSceneChanged, this, &ScenePanel::refresh);
    refresh();
}

QString ScenePanel::selectedId() const
{
    const auto items = m_list->selectedItems();
    return items.isEmpty() ? QString() : items.first()->data(Qt::UserRole).toString();
}

void ScenePanel::setPlayingScene(const QString& sceneId)
{
    if (m_playingScene == sceneId)
        return;
    m_playingScene = sceneId;
    refresh();
}

void ScenePanel::refresh()
{
    const Project& project = m_controller->project();
    const QString editing = m_controller->editingSceneId();
    {
        const QSignalBlocker blocker(m_list);
        m_list->clear();
        int index = 0;
        for (const Scene& s : project.scenes) {
            ++index;
            const QString id = QString::fromStdString(s.id);
            QString text = QStringLiteral("%1. %2").arg(index).arg(QString::fromStdString(s.name));
            if (id == m_playingScene)
                text += QStringLiteral("   ▶ 재생 중");
            auto* item = new QListWidgetItem(text, m_list);
            item->setData(Qt::UserRole, id);
            if (id == m_playingScene) {
                QFont f = item->font();
                f.setBold(true);
                item->setFont(f);
            }
            if (id == editing) {
                m_list->setCurrentItem(item);
                item->setSelected(true);
            }
        }
    }
    const Scene* scene = m_controller->editingScene();
    m_name->setText(scene ? QString::fromStdString(scene->name) : QString());
    m_removeButton->setEnabled(project.scenes.size() > 1);
}

} // namespace led
