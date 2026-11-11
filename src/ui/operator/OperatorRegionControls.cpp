#include "ui/operator/OperatorRegionControls.h"

#include "core/LayerContent.h"
#include "ui/ProjectController.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace led {

namespace {

// 장면에서 이 영역 자막 클립의 현재 문구 (없으면 nullptr)
const TickerConfig* tickerOf(const Scene* scene, const std::string& regionId)
{
    if (!scene)
        return nullptr;
    for (const TimelineLayer& layer : scene->timeline.layers)
        if (layer.regionId == regionId)
            for (const TimelineClip& clip : layer.clips)
                if (clip.kind == ClipKind::Ticker)
                    return &clip.ticker;
    return nullptr;
}

} // namespace

OperatorRegionControls::OperatorRegionControls(ProjectController* controller, QWidget* parent)
    : QWidget(parent)
    , m_controller(controller)
    , m_rows(new QVBoxLayout(this))
{
    m_rows->setContentsMargins(0, 0, 0, 0);
    m_rows->setSpacing(6);
    connect(controller, &ProjectController::projectReset, this, &OperatorRegionControls::rebuild);
    connect(controller, &ProjectController::regionsChanged, this, &OperatorRegionControls::rebuild);
    connect(controller, &ProjectController::timelineChanged, this, [this](const QString& id) {
        if (id == m_sceneId)
            rebuild();
    });
    rebuild();
}

void OperatorRegionControls::setSceneId(const QString& sceneId)
{
    if (m_sceneId == sceneId)
        return;
    m_sceneId = sceneId;
    rebuild();
}

void OperatorRegionControls::rebuild()
{
    if (m_content) {
        // 이 목록 안의 버튼에서 호출될 수 있으므로 바로 지우지 않고 숨긴 뒤 나중에 삭제
        m_rows->removeWidget(m_content);
        m_content->hide();
        m_content->deleteLater();
    }
    m_content = new QWidget(this);
    auto* layout = new QVBoxLayout(m_content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    m_rows->addWidget(m_content);

    const Project& project = m_controller->project();
    const Scene* scene = project.findScene(m_sceneId.toStdString());
    if (project.regions.empty()) {
        auto* empty = new QLabel(QStringLiteral("영역 없음"), m_content);
        empty->setObjectName("hintLabel");
        layout->addWidget(empty);
        return;
    }

    for (int i = 0; i < static_cast<int>(project.regions.size()); ++i) {
        const Region& region = project.regions[static_cast<std::size_t>(i)];
        // 명령 대상은 영역 번호 (이름의 공백/따옴표와 무관)
        const QString number = QString::number(i + 1);

        auto* row = new QWidget(m_content);
        row->setObjectName("regionRow");
        auto* rowLayout = new QVBoxLayout(row);
        rowLayout->setContentsMargins(10, 8, 10, 8);
        rowLayout->setSpacing(6);

        auto* top = new QHBoxLayout;
        auto* name = new QLabel(QString::fromStdString(region.name), row);
        name->setObjectName("regionName");
        top->addWidget(name, 1);

        const bool hasLayer = scene && playlistLayerIndex(scene->timeline, region.id) >= 0;
        if (hasLayer) {
            auto* prev = new QPushButton(QStringLiteral("◀ 이전"), row);
            auto* next = new QPushButton(QStringLiteral("다음 ▶"), row);
            prev->setToolTip(QStringLiteral("이 영역 플레이리스트의 이전 항목"));
            next->setToolTip(QStringLiteral("이 영역 플레이리스트의 다음 항목"));
            connect(prev, &QPushButton::clicked, this, [this, number] { emit commandRequested(QStringLiteral("prev ") + number); });
            connect(next, &QPushButton::clicked, this, [this, number] { emit commandRequested(QStringLiteral("next ") + number); });
            top->addWidget(prev);
            top->addWidget(next);
        }
        auto* visible = new QPushButton(row);
        visible->setObjectName("visibleToggle");
        visible->setCheckable(true);
        visible->setChecked(region.visible);
        visible->setText(region.visible ? QStringLiteral("보임") : QStringLiteral("숨김"));
        connect(visible, &QPushButton::toggled, this, [this, number, visible](bool on) {
            visible->setText(on ? QStringLiteral("보임") : QStringLiteral("숨김"));
            emit commandRequested((on ? QStringLiteral("show ") : QStringLiteral("hide ")) + number);
        });
        top->addWidget(visible);
        rowLayout->addLayout(top);

        if (const TickerConfig* ticker = tickerOf(scene, region.id)) {
            auto* tickerRow = new QHBoxLayout;
            auto* edit = new QLineEdit(row);
            QStringList lines;
            for (const auto& l : ticker->lines)
                lines << QString::fromStdString(l);
            edit->setText(lines.join(QStringLiteral(" | ")));
            edit->setPlaceholderText(QStringLiteral("자막 문구 ('|'로 여러 문구)"));
            edit->setAccessibleName(QStringLiteral("%1 자막 문구").arg(QString::fromStdString(region.name)));
            auto* apply = new QPushButton(QStringLiteral("적용"), row);
            apply->setObjectName("applyButton");
            auto send = [this, number, edit] { emit commandRequested(QStringLiteral("text %1 %2").arg(number, edit->text())); };
            connect(apply, &QPushButton::clicked, this, send);
            connect(edit, &QLineEdit::returnPressed, this, send);
            tickerRow->addWidget(edit, 1);
            tickerRow->addWidget(apply);
            rowLayout->addLayout(tickerRow);
        }
        layout->addWidget(row);
    }

    auto* reset = new QPushButton(QStringLiteral("외부 변경 되돌리기 (자막/보임)"), m_content);
    reset->setToolTip(QStringLiteral("운영 화면이나 외부 명령으로 바꾼 자막 문구와 영역 보임/숨김을 프로젝트 설정으로 되돌림"));
    connect(reset, &QPushButton::clicked, this, [this] {
        emit commandRequested(QStringLiteral("reset"));
        rebuild();
    });
    layout->addWidget(reset);
    layout->addStretch(1); // 줄이 늘어나지 않게 위로 모음
}

} // namespace led
