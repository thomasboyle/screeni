#pragma once

#include "BarChart.h"
#include "tracker.h"

#include <QWidget>

class QLabel;
class QListWidget;
class QPushButton;

class OverviewPage : public QWidget {
    Q_OBJECT
public:
    explicit OverviewPage(Tracker& tracker, QWidget* parent = nullptr);
public slots:
    void refresh();
signals:
    void dayRangeSelected();
    void weekRangeSelected();
private:
    enum class Range { Day, Week };
    void setRange(Range r);
    static QString formatDuration(qint64 ms);
    Tracker& tracker_;
    Range range_ = Range::Day;
    QLabel* rangeLabel_ = nullptr;
    QPushButton* dayBtn_ = nullptr;
    QPushButton* weekBtn_ = nullptr;
    BarChart* chart_ = nullptr;
    QListWidget* appsList_ = nullptr;
};
