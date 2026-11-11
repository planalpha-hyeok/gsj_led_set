#pragma once

// 운영 화면의 진행 막대: 장면 진행 위치와 큐 지점 표시 (표시만, 클릭으로 이동하지 않음 - 실수 방지)

#include <QWidget>

#include <vector>

namespace led {

class CueProgressBar : public QWidget {
    Q_OBJECT

public:
    explicit CueProgressBar(QWidget* parent = nullptr);

    // length 0 이하 = 끝없이 재생 (막대 대신 안내 표시)
    void setProgress(double time, double length, const std::vector<double>& cues, double nextCue);

    QSize sizeHint() const override { return QSize(300, 40); }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    double m_time = 0.0;
    double m_length = 0.0;
    std::vector<double> m_cues;
    double m_nextCue = -1.0;
};

} // namespace led
