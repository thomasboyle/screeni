#pragma once

#include "BarChart.h"
#include "tracker.h"

#include <QWidget>

class QLabel;
class QProgressBar;

class InsightsPage : public QWidget {
    Q_OBJECT
public:
    explicit InsightsPage(Tracker& tracker, QWidget* parent = nullptr);
public slots:
    void refresh();
private:
    static QString formatPercent(double share);
    static QString formatHour(int hour);
    Tracker& tracker_;
    QLabel* weekTotal_ = nullptr;
    QLabel* dailyAvg_ = nullptr;
    QLabel* weekDelta_ = nullptr;
    QLabel* weekDeltaDetail_ = nullptr;
    QLabel* activeApps_ = nullptr;
    QLabel* busiest_ = nullptr;
    QLabel* quietest_ = nullptr;
    QLabel* peakHour_ = nullptr;
    QLabel* peakDetail_ = nullptr;
    QLabel* dominant_ = nullptr;
    QLabel* dominantDetail_ = nullptr;
    QLabel* morning_ = nullptr;
    QLabel* afternoon_ = nullptr;
    QLabel* evening_ = nullptr;
    QLabel* night_ = nullptr;
    QProgressBar* morningBar_ = nullptr;
    QProgressBar* afternoonBar_ = nullptr;
    QProgressBar* eveningBar_ = nullptr;
    QProgressBar* nightBar_ = nullptr;
    BarChart* trend_ = nullptr;
};
