#include "BarChart.h"

#include "theme.h"

#include <QPainter>
#include <algorithm>
#include <cmath>

BarChart::BarChart(QWidget* parent) : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setToolTipDuration(3000);
}

void BarChart::setBars(const QVector<ChartBarData>& bars)
{
    bars_ = bars;
    updateGeometry();
    update();
}

QSize BarChart::sizeHint() const
{
    const int n = std::max(1, static_cast<int>(bars_.size()));
    const int w = n * (barWidth_ + 4) + axisWidth_ + 8;
    return QSize(w, plotHeight_ + 40);
}

QSize BarChart::minimumSizeHint() const
{
    return QSize(200, plotHeight_ + 40);
}

void BarChart::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const int plotTop = 8;
    const int labelH = 20;
    const int plotBottom = height() - labelH - 4;
    const int h = std::max(1, plotBottom - plotTop);
    const int plotW = width() - axisWidth_ - 8;

    p.setPen(QPen(QColor(0, 0, 0, 0x33), 1));
    p.drawLine(0, plotTop + h / 5, plotW, plotTop + h / 5);
    p.drawLine(0, plotTop + h / 2, plotW, plotTop + h / 2);
    p.setPen(QPen(Theme::BorderSoft, 1));
    p.drawLine(0, plotBottom, plotW, plotBottom);

    qint64 maxVal = 1;
    for (const auto& b : bars_)
        maxVal = std::max(maxVal, b.durationMs);

    const int n = bars_.size();
    if (n <= 0)
        return;

    const double slot = static_cast<double>(plotW) / n;
    for (int i = 0; i < n; ++i) {
        const double share = static_cast<double>(bars_[i].durationMs) / static_cast<double>(maxVal);
        const double barH = std::max(3.0, share * h);
        const double x = i * slot + (slot - barWidth_) * 0.5;
        const double y = plotBottom - barH;
        QRectF r(x, y, barWidth_, barH);
        p.setPen(QPen(Theme::AccentDeep, 1));
        p.setBrush(Theme::Accent);
        p.drawRoundedRect(r, 3, 3);
        if (!bars_[i].label.isEmpty()) {
            p.setPen(Theme::InkMuted);
            QFont f = font();
            f.setPixelSize(12);
            p.setFont(f);
            p.drawText(QRectF(i * slot, plotBottom + 2, slot, labelH),
                       Qt::AlignHCenter | Qt::AlignTop, bars_[i].label);
        }
    }

    p.setPen(Theme::InkMuted);
    QFont f = font();
    f.setPixelSize(12);
    p.setFont(f);
    const int ax = plotW + 6;
    p.drawText(ax, plotTop + 4, QStringLiteral("60m"));
    p.drawText(ax, plotTop + h / 2 + 4, QStringLiteral("30m"));
    p.drawText(ax, plotBottom, QStringLiteral("0m"));
}
