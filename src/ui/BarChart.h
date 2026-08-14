#pragma once

#include <QWidget>
#include <QVector>
#include <QString>

struct ChartBarData {
    QString label;
    qint64 durationMs = 0;
    QString tooltip;
};

class BarChart : public QWidget {
    Q_OBJECT
public:
    explicit BarChart(QWidget* parent = nullptr);
    void setBars(const QVector<ChartBarData>& bars);
    void setPlotHeight(int h) { plotHeight_ = h; update(); }
    void setBarWidth(int w) { barWidth_ = w; update(); }
protected:
    void paintEvent(QPaintEvent* event) override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
private:
    QVector<ChartBarData> bars_;
    int plotHeight_ = 150;
    int barWidth_ = 13;
    int axisWidth_ = 45;
};
