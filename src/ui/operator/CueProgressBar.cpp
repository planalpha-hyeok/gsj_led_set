#include "ui/operator/CueProgressBar.h"

#include "core/ControlCommand.h"
#include "ui/Palette.h"

#include <QPainter>

#include <algorithm>
#include <cmath>

namespace led {

CueProgressBar::CueProgressBar(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(40);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void CueProgressBar::setProgress(double time, double length, const std::vector<double>& cues, double nextCue)
{
    if (std::fabs(time - m_time) < 0.02 && length == m_length && cues == m_cues && nextCue == m_nextCue)
        return;
    m_time = time;
    m_length = length;
    m_cues = cues;
    m_nextCue = nextCue;
    update();
}

void CueProgressBar::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    QFont mono(palette::monoFontFamily());
    mono.setPixelSize(12);
    p.setFont(mono);

    const QRect textRow(0, 0, width(), 16);
    p.setPen(palette::kTextDim);
    p.drawText(textRow, Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("0:00"));
    if (m_length <= 0.0) {
        p.drawText(textRow, Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("끝없이 반복"));
        const QRectF bar(0, 24, width(), 8);
        p.setPen(Qt::NoPen);
        p.setBrush(palette::kBorderStrong);
        p.drawRoundedRect(bar, 4, 4);
        return;
    }
    p.drawText(textRow, Qt::AlignRight | Qt::AlignVCenter, QString::fromStdString(formatTimeText(m_length)));

    const QRectF bar(0, 24, width(), 8);
    p.setPen(Qt::NoPen);
    p.setBrush(palette::kBorderStrong);
    p.drawRoundedRect(bar, 4, 4);
    const double ratio = std::clamp(m_time / m_length, 0.0, 1.0);
    p.setBrush(palette::kLive);
    p.drawRoundedRect(QRectF(bar.x(), bar.y(), bar.width() * ratio, bar.height()), 4, 4);

    for (double cue : m_cues) {
        if (cue < 0.0 || cue > m_length)
            continue;
        const double x = bar.x() + bar.width() * (cue / m_length);
        const bool next = std::fabs(cue - m_nextCue) < 1e-6;
        p.setBrush(next ? palette::kSelect : palette::kTextDim);
        p.drawRect(QRectF(x - 1.0, bar.y() - 5, 2.0, bar.height() + 10));
    }
}

} // namespace led
