#pragma once

// 타임라인 속성: 선택한 클립(여러 개 가능) / 레이어 / 큐

#include "core/Project.h"

#include <QWidget>

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QStackedWidget;

namespace led {

class TimelinePropertyPanel : public QWidget {
    Q_OBJECT

public:
    explicit TimelinePropertyPanel(QWidget* parent = nullptr);

    // 화면 갱신 (선택 상태에 따라 클립/레이어/큐/안내 표시)
    void display(const Timeline& timeline, const std::set<std::string>& clips, int layer, const std::string& cue,
              const std::vector<std::pair<std::string, std::string>>& regions);

signals:
    void edited(const Timeline& timeline);

private slots:
    void commitClips();
    void commitLayer();
    void commitCue();
    void browseSource();

private:
    Timeline m_tl;
    std::set<std::string> m_clips;
    int m_layer = -1;
    std::string m_cue;
    bool m_loading = false;

    QStackedWidget* m_stack;
    // 클립
    QLabel* m_clipTitle;
    QLineEdit* m_source;
    QPushButton* m_browse;
    QDoubleSpinBox* m_start;
    QDoubleSpinBox* m_duration;
    QDoubleSpinBox* m_inPoint;
    QDoubleSpinBox* m_fadeIn;
    QDoubleSpinBox* m_fadeOut;
    QSpinBox* m_opacity;
    QSpinBox* m_volume;
    QCheckBox* m_loop;
    QLabel* m_mediaInfo;
    // 레이어
    QLineEdit* m_layerName;
    QComboBox* m_layerRegion;
    QCheckBox* m_layerVisible;
    QCheckBox* m_layerSound;
    QCheckBox* m_layerLocked;
    // 큐
    QLineEdit* m_cueName;
    QDoubleSpinBox* m_cueTime;
};

} // namespace led
