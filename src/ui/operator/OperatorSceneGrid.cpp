#include "ui/operator/OperatorSceneGrid.h"

#include "ui/ProjectController.h"

#include <QGridLayout>
#include <QKeySequence>
#include <QPushButton>
#include <QShortcut>
#include <QStyle>

namespace led {

namespace {
constexpr int kColumns = 4;
constexpr int kShortcutCount = 9;
}

OperatorSceneGrid::OperatorSceneGrid(ProjectController* controller, QWidget* parent)
    : QWidget(parent)
    , m_controller(controller)
    , m_grid(new QGridLayout(this))
{
    m_grid->setContentsMargins(0, 0, 0, 0);
    m_grid->setSpacing(10);
    connect(controller, &ProjectController::projectReset, this, &OperatorSceneGrid::rebuild);
    connect(controller, &ProjectController::scenesChanged, this, &OperatorSceneGrid::rebuild);
    rebuild();
}

void OperatorSceneGrid::rebuild()
{
    for (QPushButton* b : m_buttons) {
        m_grid->removeWidget(b);
        b->hide();
        b->deleteLater();
    }
    m_buttons.clear();
    // 단축키는 바로 삭제 (남아 있으면 같은 키가 2개가 되어 둘 다 동작하지 않음)
    for (QShortcut* s : m_shortcuts)
        delete s;
    m_shortcuts.clear();

    const auto& scenes = m_controller->project().scenes;
    for (int i = 0; i < static_cast<int>(scenes.size()); ++i) {
        const Scene& scene = scenes[static_cast<std::size_t>(i)];
        const QString id = QString::fromStdString(scene.id);
        const QString key = i < kShortcutCount ? QStringLiteral("Ctrl+%1").arg(i + 1) : QStringLiteral("%1").arg(i + 1);
        auto* button = new QPushButton(QStringLiteral("%1\n%2").arg(key, QString::fromStdString(scene.name)), this);
        button->setObjectName("sceneButton");
        button->setToolTip(QStringLiteral("누르면 '%1' 장면으로 바로 전환").arg(QString::fromStdString(scene.name)));
        button->setProperty("sceneId", id);
        connect(button, &QPushButton::clicked, this, [this, id] { emit playSceneRequested(id); });
        m_grid->addWidget(button, i / kColumns, i % kColumns);
        m_buttons.push_back(button);

        if (i < kShortcutCount) {
            // 운영 화면이 보일 때만 동작 (숨겨진 위젯의 단축키는 동작하지 않음)
            // 출력 창이 선택되어 있어도 동작하도록 프로그램 전체 단축키
            auto* shortcut = new QShortcut(QKeySequence(Qt::CTRL | static_cast<Qt::Key>(Qt::Key_1 + i)), this);
            shortcut->setContext(Qt::ApplicationShortcut);
            connect(shortcut, &QShortcut::activated, this, [this, id] { emit playSceneRequested(id); });
            m_shortcuts.push_back(shortcut);
        }
    }
    for (int c = 0; c < kColumns; ++c)
        m_grid->setColumnStretch(c, 1);
    updateHighlight();
}

void OperatorSceneGrid::setLiveScene(const QString& sceneId)
{
    if (m_liveScene == sceneId)
        return;
    m_liveScene = sceneId;
    updateHighlight();
}

void OperatorSceneGrid::updateHighlight()
{
    for (QPushButton* b : m_buttons) {
        const bool live = !m_liveScene.isEmpty() && b->property("sceneId").toString() == m_liveScene;
        if (b->property("live").toBool() == live)
            continue;
        b->setProperty("live", live);
        b->style()->unpolish(b);
        b->style()->polish(b);
    }
}

} // namespace led
