#pragma once

// 편집 중인 프로젝트를 보관하고, 변경 시 화면들에 알림

#include "core/Project.h"

#include <QObject>
#include <QString>

namespace led {

class ProjectController : public QObject {
    Q_OBJECT

public:
    explicit ProjectController(QObject* parent = nullptr);

    const Project& project() const { return m_project; }
    QString filePath() const { return m_filePath; }
    bool isDirty() const { return m_dirty; }

    void newProject();
    bool load(const QString& path, QString* error);
    bool save(const QString& path, QString* error);

    // 영역
    QString addRegion();
    void removeRegion(const QString& id);
    void updateRegion(const Region& region);
    void moveRegion(const QString& id, int delta); // +1: 앞으로, -1: 뒤로
    QString selectedRegionId() const { return m_selectedRegion; }
    void selectRegion(const QString& id);

    // 출력
    QString addOutput();
    void removeOutput(const QString& id);
    void updateOutput(const OutputConfig& output);

    // 캔버스
    void setCanvasSize(int width, int height);

    // 장면
    QString editingSceneId() const { return m_editingScene; }
    const Scene* editingScene() const;
    void setEditingScene(const QString& id);
    QString addScene(SceneKind kind = SceneKind::Regions);
    void setSceneKind(const QString& id, SceneKind kind);
    // 타임라인 장면 편집
    void setTimeline(const QString& sceneId, const Timeline& timeline);
    QString duplicateScene(const QString& id);
    void renameScene(const QString& id, const QString& name);
    bool removeScene(const QString& id); // 마지막 장면은 삭제 불가
    void moveScene(const QString& id, int delta);

    // 영역 콘텐츠 (편집 중인 장면 기준)
    const RegionContent* contentFor(const QString& sceneId, const QString& regionId) const;
    void setContent(const QString& sceneId, const RegionContent& content);

    // 스케줄 / 운영
    void setSchedule(const Schedule& schedule);
    void setScheduleEnabled(bool enabled);
    void setAutoStart(bool enabled);

    // 재생 설정
    void setHardwareDecoding(bool enabled);
    void setAudioDevice(const QString& name);

    // 인터랙티브
    void setInteractive(const InteractiveConfig& config);

signals:
    void projectReset();
    void regionsChanged();
    void outputsChanged();
    void canvasChanged();
    void dirtyChanged(bool dirty);
    void regionSelected(const QString& id);
    void scenesChanged();
    void editingSceneChanged(const QString& id);
    void contentChanged(const QString& sceneId, const QString& regionId);
    void scheduleChanged();
    void playbackSettingsChanged();
    void interactiveChanged();
    void timelineChanged(const QString& sceneId);

private:
    void setDirty(bool dirty);

    Project m_project;
    QString m_filePath;
    QString m_selectedRegion;
    QString m_editingScene;
    bool m_dirty = false;
};

} // namespace led
