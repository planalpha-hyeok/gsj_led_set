#pragma once

// 라이브 입력(캡처보드/웹캠) 선택 대화상자

#include "core/Project.h"

#include <QDialog>

class QComboBox;
class QDoubleSpinBox;
class QLabel;

namespace led {

class LiveInputDialog : public QDialog {
    Q_OBJECT

public:
    explicit LiveInputDialog(QWidget* parent = nullptr);

    // 기존 항목 편집 시 값 채우기
    void setItem(const PlaylistItem& item);
    // 선택 결과 (type = Live)
    PlaylistItem item() const;

private slots:
    void refreshDevices();

private:
    QComboBox* m_device;
    QComboBox* m_size;
    QDoubleSpinBox* m_fps;
    QLabel* m_status;
    PlaylistItem m_item;
};

} // namespace led
