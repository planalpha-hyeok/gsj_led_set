#include "ui/ContentEditor.h"

#include "core/Project.h"
#include "ui/PlaylistEditor.h"
#include "ui/ProjectController.h"
#include "ui/TickerEditor.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace led {

ContentEditor::ContentEditor(ProjectController* controller, QWidget* parent)
    : QWidget(parent)
    , m_controller(controller)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    m_sceneLabel = new QLabel(this);
    m_sceneLabel->setObjectName("hintLabel");
    layout->addWidget(m_sceneLabel);

    auto* kindRow = new QHBoxLayout;
    kindRow->addWidget(new QLabel(QStringLiteral("종류"), this));
    m_kind = new QComboBox(this);
    m_kind->addItem(QStringLiteral("플레이리스트 (영상/이미지)"), int(ContentKind::Playlist));
    m_kind->addItem(QStringLiteral("흐르는 자막"), int(ContentKind::Ticker));
    kindRow->addWidget(m_kind, 1);
    layout->addLayout(kindRow);

    m_stack = new QStackedWidget(this);
    m_playlist = new PlaylistEditor(m_stack);
    m_ticker = new TickerEditor(m_stack);
    m_stack->addWidget(m_playlist);
    m_stack->addWidget(m_ticker);
    layout->addWidget(m_stack, 1);

    connect(m_kind, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ContentEditor::onKindChanged);
    connect(m_playlist, &PlaylistEditor::edited, this, &ContentEditor::applyPlaylist);
    connect(m_ticker, &TickerEditor::edited, this, &ContentEditor::applyTicker);

    connect(controller, &ProjectController::projectReset, this, &ContentEditor::reload);
    connect(controller, &ProjectController::regionSelected, this, &ContentEditor::reload);
    connect(controller, &ProjectController::editingSceneChanged, this, &ContentEditor::reload);
    connect(controller, &ProjectController::scenesChanged, this, &ContentEditor::reload);
    connect(controller, &ProjectController::regionsChanged, this, &ContentEditor::reload);
    connect(controller, &ProjectController::contentChanged, this, [this] {
        if (!m_applying) // 이 화면에서 바꾼 내용은 다시 불러오지 않음 (입력 중 커서 유지)
            reload();
    });
    reload();
}

void ContentEditor::reload()
{
    const QString sceneId = m_controller->editingSceneId();
    const QString regionId = m_controller->selectedRegionId();
    const Scene* scene = m_controller->editingScene();
    const Region* region = m_controller->project().findRegion(regionId.toStdString());
    const bool valid = scene && region;
    setEnabled(valid);
    if (!valid) {
        m_sceneLabel->setText(scene ? QStringLiteral("편집 중인 장면: %1").arg(QString::fromStdString(scene->name))
                                    : QString());
        return;
    }
    const bool editable = m_controller->isContentEditable(sceneId, regionId);
    if (!editable) {
        m_sceneLabel->setText(QStringLiteral("장면 '%1' / 영역 '%2'의 첫 레이어는 타임라인에서 자유롭게 배치되어 있습니다.\n"
                                             "아래 '타임라인' 창에서 편집하세요.")
                                  .arg(QString::fromStdString(scene->name), QString::fromStdString(region->name)));
    } else {
        m_sceneLabel->setText(QStringLiteral("편집 중인 장면: %1  (타임라인의 '%2' 첫 레이어)")
                                  .arg(QString::fromStdString(scene->name), QString::fromStdString(region->name)));
    }
    m_kind->setEnabled(editable);
    m_stack->setEnabled(editable);

    const RegionContent content = currentContent();
    m_loading = true;
    m_kind->setCurrentIndex(content.kind == ContentKind::Ticker ? 1 : 0);
    m_stack->setCurrentIndex(content.kind == ContentKind::Ticker ? 1 : 0);
    m_playlist->setContent(content);
    m_ticker->setContent(content);
    m_loading = false;
}

RegionContent ContentEditor::currentContent() const
{
    const QString regionId = m_controller->selectedRegionId();
    RegionContent content = m_controller->contentFor(m_controller->editingSceneId(), regionId);
    content.regionId = regionId.toStdString();
    return content;
}

void ContentEditor::onKindChanged(int index)
{
    if (m_loading)
        return;
    RegionContent content = currentContent();
    const auto kind = static_cast<ContentKind>(m_kind->itemData(index).toInt());
    // 종류를 바꾸면 레이어의 클립이 바뀜: 내용이 있으면 확인
    if (!content.isEmpty()) {
        const auto answer = QMessageBox::question(
            this, QStringLiteral("종류 변경"),
            QStringLiteral("종류를 바꾸면 이 영역 첫 레이어의 현재 내용(%1)이 지워집니다. 계속하시겠습니까?")
                .arg(content.kind == ContentKind::Ticker ? QStringLiteral("자막")
                                                         : QStringLiteral("항목 %1개").arg(content.items.size())));
        if (answer != QMessageBox::Yes) {
            reload();
            return;
        }
    }
    m_stack->setCurrentIndex(index);
    content.kind = kind;
    content.items.clear();
    if (kind == ContentKind::Ticker)
        content.ticker = TickerConfig{};
    content.loopPlaylist = true;
    store(content);
    reload();
}

// 각 편집기는 자기 담당 부분만 반영 (다른 부분은 저장된 최신 값 유지)
void ContentEditor::applyPlaylist(const RegionContent& edited)
{
    RegionContent content = currentContent();
    content.items = edited.items;
    content.loopPlaylist = edited.loopPlaylist;
    store(content);
}

void ContentEditor::applyTicker(const RegionContent& edited)
{
    RegionContent content = currentContent();
    content.ticker = edited.ticker;
    store(content);
}

void ContentEditor::store(const RegionContent& content)
{
    m_applying = true;
    m_controller->setContent(m_controller->editingSceneId(), content);
    m_applying = false;
}

} // namespace led
