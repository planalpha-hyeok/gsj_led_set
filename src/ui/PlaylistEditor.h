#pragma once

// 플레이리스트 편집 (항목 추가/삭제/순서, 재생 시간, 전환 효과, 목록 반복)

#include "core/Project.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QListWidget;
class QPushButton;

namespace led {

class PlaylistEditor : public QWidget {
    Q_OBJECT

public:
    explicit PlaylistEditor(QWidget* parent = nullptr);

    // 화면을 content로 다시 채움 (선택된 항목은 유지)
    void setContent(const RegionContent& content);

signals:
    void edited(const RegionContent& content);

private slots:
    void addItems();
    void addLiveInput();
    void editLiveInput();
    void removeItem();
    void moveItem(int delta);
    void onSelectionChanged();
    void commitItem();
    void commitLoop();

private:
    int currentIndex() const;
    void refreshList(const std::string& selectId);
    void loadItem();

    RegionContent m_content;
    QListWidget* m_list;
    QPushButton* m_addButton;
    QPushButton* m_addLiveButton;
    QPushButton* m_liveSettingsButton;
    QPushButton* m_removeButton;
    QPushButton* m_upButton;
    QPushButton* m_downButton;
    QWidget* m_itemForm;
    QDoubleSpinBox* m_duration;
    QComboBox* m_transition;
    QDoubleSpinBox* m_fade;
    QCheckBox* m_loop;
    bool m_loading = false;
};

} // namespace led
