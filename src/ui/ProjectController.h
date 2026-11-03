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

signals:
    void projectReset();
    void regionsChanged();
    void outputsChanged();
    void canvasChanged();
    void dirtyChanged(bool dirty);
    void regionSelected(const QString& id);

private:
    void setDirty(bool dirty);

    Project m_project;
    QString m_filePath;
    QString m_selectedRegion;
    bool m_dirty = false;
};

} // namespace led
