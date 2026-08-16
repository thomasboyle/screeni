#include "ui/InsightsPage.h"

#include "theme.h"
#include "format.h"

#include <QDate>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QScrollArea>
#include <QVBoxLayout>

#include <array>
#include <cmath>

QString InsightsPage::formatPercent(double share)
{
    return QStringLiteral("%1%").arg(static_cast<int>(std::round(share * 100.0)));
}

QString InsightsPage::formatHour(int hour)
{
    if (hour == 0)
        return QStringLiteral("12am");
    if (hour == 12)
        return QStringLiteral("12pm");
    return hour < 12 ? QStringLiteral("%1am").arg(hour) : QStringLiteral("%1pm").arg(hour - 12);
}

InsightsPage::InsightsPage(Tracker& tracker, QWidget* parent)
    : QWidget(parent)
    , tracker_(tracker)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* content = new QWidget;
    scroll->setWidget(content);
    outer->addWidget(scroll);

    auto* lay = new QVBoxLayout(content);
    lay->setContentsMargins(40, 34, 40, 24);
    lay->setSpacing(16);

    auto* title = new QLabel(QStringLiteral("Insights"));
    title->setObjectName(QStringLiteral("title"));
    lay->addWidget(title);

    auto* subtitle = new QLabel(
        QStringLiteral("Computed from your tracked usage this week (Mon–today)."));
    subtitle->setObjectName(QStringLiteral("muted"));
    subtitle->setStyleSheet(QStringLiteral("font-size: 13px;"));
    lay->addWidget(subtitle);

    auto makeCard = [](QWidget* parent, QVBoxLayout*& layOut) {
        auto* card = new QFrame(parent);
        card->setObjectName(QStringLiteral("card"));
        layOut = new QVBoxLayout(card);
        layOut->setContentsMargins(18, 16, 18, 16);
        layOut->setSpacing(6);
        return card;
    };

    auto* topRow = new QHBoxLayout;
    topRow->setSpacing(12);
    QVBoxLayout* weekLay = nullptr;
    QVBoxLayout* deltaLay = nullptr;
    QVBoxLayout* activeLay = nullptr;
    topRow->addWidget(makeCard(content, weekLay));
    topRow->addWidget(makeCard(content, deltaLay));
    topRow->addWidget(makeCard(content, activeLay));
    lay->addLayout(topRow);

    auto caption = [](const QString& text) {
        auto* l = new QLabel(text);
        l->setObjectName(QStringLiteral("muted"));
        l->setStyleSheet(QStringLiteral("font-size: 12px;"));
        return l;
    };

    weekTotal_ = new QLabel(QStringLiteral("0m"));
    weekTotal_->setObjectName(QStringLiteral("display"));
    weekTotal_->setStyleSheet(QStringLiteral("font-size: 28px;"));
    dailyAvg_ = new QLabel(QStringLiteral("0m"));
    dailyAvg_->setObjectName(QStringLiteral("muted"));
    dailyAvg_->setStyleSheet(QStringLiteral("font-size: 13px;"));
    weekLay->addWidget(caption(QStringLiteral("This week")));
    weekLay->addWidget(weekTotal_);
    weekLay->addWidget(dailyAvg_);
    weekLay->addWidget(caption(QStringLiteral("avg / day so far")));

    weekDelta_ = new QLabel(QStringLiteral("—"));
    weekDelta_->setObjectName(QStringLiteral("display"));
    weekDelta_->setStyleSheet(QStringLiteral("font-size: 28px;"));
    weekDeltaDetail_ = new QLabel(QStringLiteral("Compared with the same days last week"));
    weekDeltaDetail_->setObjectName(QStringLiteral("muted"));
    weekDeltaDetail_->setStyleSheet(QStringLiteral("font-size: 12px;"));
    weekDeltaDetail_->setWordWrap(true);
    deltaLay->addWidget(caption(QStringLiteral("vs last week")));
    deltaLay->addWidget(weekDelta_);
    deltaLay->addWidget(weekDeltaDetail_);

    activeApps_ = new QLabel(QStringLiteral("0"));
    activeApps_->setObjectName(QStringLiteral("display"));
    activeApps_->setStyleSheet(QStringLiteral("font-size: 28px;"));
    activeLay->addWidget(caption(QStringLiteral("Active apps")));
    activeLay->addWidget(activeApps_);
    activeLay->addWidget(caption(QStringLiteral("≥ 1 minute this week")));

    auto* midRow = new QHBoxLayout;
    midRow->setSpacing(12);
    QVBoxLayout* extremesLay = nullptr;
    QVBoxLayout* peakLay = nullptr;
    midRow->addWidget(makeCard(content, extremesLay), 1);
    midRow->addWidget(makeCard(content, peakLay), 1);
    lay->addLayout(midRow);

    auto* extremesTitle = new QLabel(QStringLiteral("This week extremes"));
    extremesTitle->setObjectName(QStringLiteral("section"));
    extremesTitle->setStyleSheet(QStringLiteral("font-size: 15px;"));
    extremesLay->addWidget(extremesTitle);
    auto* extremesRow = new QHBoxLayout;
    extremesRow->setSpacing(16);
    auto* busyCol = new QVBoxLayout;
    auto* quietCol = new QVBoxLayout;
    busyCol->setSpacing(4);
    quietCol->setSpacing(4);
    extremesRow->addLayout(busyCol);
    extremesRow->addLayout(quietCol);
    extremesLay->addLayout(extremesRow);
    busyCol->addWidget(caption(QStringLiteral("Busiest")));
    busiest_ = new QLabel(QStringLiteral("—"));
    busiest_->setStyleSheet(QStringLiteral("font-size: 15px;"));
    busyCol->addWidget(busiest_);
    quietCol->addWidget(caption(QStringLiteral("Quietest")));
    quietest_ = new QLabel(QStringLiteral("—"));
    quietest_->setStyleSheet(QStringLiteral("font-size: 15px;"));
    quietCol->addWidget(quietest_);

    auto* peakTitle = new QLabel(QStringLiteral("Peak hour today"));
    peakTitle->setObjectName(QStringLiteral("section"));
    peakTitle->setStyleSheet(QStringLiteral("font-size: 15px;"));
    peakLay->addWidget(peakTitle);
    peakHour_ = new QLabel(QStringLiteral("—"));
    peakHour_->setObjectName(QStringLiteral("display"));
    peakHour_->setStyleSheet(QStringLiteral("font-size: 28px;"));
    peakDetail_ = new QLabel(QStringLiteral("No usage recorded today"));
    peakDetail_->setObjectName(QStringLiteral("muted"));
    peakDetail_->setStyleSheet(QStringLiteral("font-size: 12px;"));
    peakLay->addWidget(peakHour_);
    peakLay->addWidget(peakDetail_);

    QVBoxLayout* timeLay = nullptr;
    auto* timeCard = makeCard(content, timeLay);
    lay->addWidget(timeCard);
    auto* timeTitle = new QLabel(QStringLiteral("Time of day (Mon–today)"));
    timeTitle->setObjectName(QStringLiteral("section"));
    timeTitle->setStyleSheet(QStringLiteral("font-size: 15px;"));
    timeLay->addWidget(timeTitle);
    auto* timeRow = new QHBoxLayout;
    timeRow->setSpacing(12);

    auto makeSlot = [&](QProgressBar*& bar, QLabel*& value, const QString& label) {
        auto* col = new QVBoxLayout;
        col->setSpacing(6);
        col->addWidget(caption(label));
        bar = new QProgressBar;
        bar->setTextVisible(false);
        bar->setRange(0, 1000);
        bar->setValue(0);
        col->addWidget(bar);
        value = new QLabel(QStringLiteral("0%"));
        col->addWidget(value);
        timeRow->addLayout(col, 1);
    };

    makeSlot(morningBar_, morning_, QStringLiteral("Morning"));
    makeSlot(afternoonBar_, afternoon_, QStringLiteral("Afternoon"));
    makeSlot(eveningBar_, evening_, QStringLiteral("Evening"));
    makeSlot(nightBar_, night_, QStringLiteral("Night"));
    timeLay->addLayout(timeRow);

    QVBoxLayout* domLay = nullptr;
    auto* domCard = makeCard(content, domLay);
    lay->addWidget(domCard);
    auto* domTitle = new QLabel(QStringLiteral("Dominant app this week"));
    domTitle->setObjectName(QStringLiteral("section"));
    domTitle->setStyleSheet(QStringLiteral("font-size: 15px;"));
    domLay->addWidget(domTitle);
    dominant_ = new QLabel(QStringLiteral("—"));
    dominant_->setObjectName(QStringLiteral("display"));
    dominant_->setStyleSheet(QStringLiteral("font-size: 22px;"));
    dominant_->setWordWrap(true);
    dominantDetail_ = new QLabel(QStringLiteral("No apps tracked this week"));
    dominantDetail_->setObjectName(QStringLiteral("muted"));
    dominantDetail_->setStyleSheet(QStringLiteral("font-size: 13px;"));
    domLay->addWidget(dominant_);
    domLay->addWidget(dominantDetail_);

    QVBoxLayout* trendLay = nullptr;
    auto* trendCard = makeCard(content, trendLay);
    trendLay->setSpacing(12);
    lay->addWidget(trendCard);
    auto* trendTitle = new QLabel(QStringLiteral("Last 14 days"));
    trendTitle->setObjectName(QStringLiteral("section"));
    trendTitle->setStyleSheet(QStringLiteral("font-size: 15px;"));
    trendLay->addWidget(trendTitle);
    trend_ = new BarChart;
    trend_->setPlotHeight(96);
    trend_->setBarWidth(14);
    trend_->setMinimumHeight(150);
    trendLay->addWidget(trend_);

    refresh();
}

void InsightsPage::refresh()
{
    const QDate today = QDate::currentDate();
    const int weekIndex = (today.dayOfWeek() + 6) % 7;  // Mon=0 … Sun=6
    const QDate thisMonday = today.addDays(-weekIndex);
    const int daysElapsed = weekIndex + 1;

    // Daily totals drive the week comparison and the 14-day trend; hourly data
    // is only needed for the current week (dayparts + peak hour).
    const auto days = tracker_.store().day_totals(
        localDayString(today.addDays(-13)).toStdString(), localDayString(today).toStdString());
    const auto hours = tracker_.store().hourly_totals_range(
        localDayString(thisMonday).toStdString(), localDayString(today).toStdString());

    std::array<qint64, 14> dayTotals{};
    for (size_t d = 0; d < days.size() && d < 14; ++d)
        dayTotals[d] = days[d];

    std::array<std::array<qint64, 24>, 7> h{};
    for (size_t d = 0; d < hours.size() && d < 7; ++d)
        for (int hh = 0; hh < 24; ++hh)
            h[d][hh] = hours[d][static_cast<size_t>(hh)];

    auto dayTotal = [&](int idx) -> qint64 {
        return (idx >= 0 && idx < 14) ? dayTotals[idx] : 0;
    };

    std::array<qint64, 7> thisWeek{};
    std::array<qint64, 7> lastWeek{};
    for (int i = 0; i < 7; ++i) {
        thisWeek[i] = dayTotal(13 - weekIndex + i);
        lastWeek[i] = dayTotal(6 - weekIndex + i);
    }

    qint64 thisWeekToDate = 0;
    qint64 lastWeekToDate = 0;
    for (int i = 0; i < daysElapsed; ++i) {
        thisWeekToDate += thisWeek[i];
        lastWeekToDate += lastWeek[i];
    }

    weekTotal_->setText(formatDuration(thisWeekToDate));
    dailyAvg_->setText(formatDuration(thisWeekToDate / daysElapsed));

    if (lastWeekToDate <= 0 && thisWeekToDate <= 0) {
        weekDelta_->setText(QStringLiteral("—"));
        weekDeltaDetail_->setText(QStringLiteral("No data for this span last week"));
    } else if (lastWeekToDate <= 0) {
        weekDelta_->setText(QStringLiteral("new"));
        weekDeltaDetail_->setText(QStringLiteral("No comparable usage last week"));
    } else {
        const double delta = (thisWeekToDate - lastWeekToDate) / static_cast<double>(lastWeekToDate);
        const int pct = static_cast<int>(std::round(delta * 100.0));
        weekDelta_->setText(pct > 0 ? QStringLiteral("+%1%").arg(pct) : QStringLiteral("%1%").arg(pct));
        weekDeltaDetail_->setText(QStringLiteral("Same weekdays vs last week"));
    }

    static const QStringList dayNames = {QStringLiteral("Mon"), QStringLiteral("Tue"),
                                         QStringLiteral("Wed"), QStringLiteral("Thu"),
                                         QStringLiteral("Fri"), QStringLiteral("Sat"),
                                         QStringLiteral("Sun")};

    int busiestIdx = 0;
    int quietestIdx = 0;
    for (int i = 1; i < daysElapsed; ++i) {
        if (thisWeek[i] > thisWeek[busiestIdx])
            busiestIdx = i;
        if (thisWeek[i] < thisWeek[quietestIdx])
            quietestIdx = i;
    }

    if (thisWeekToDate <= 0) {
        busiest_->setText(QStringLiteral("—"));
        quietest_->setText(QStringLiteral("—"));
    } else {
        busiest_->setText(QStringLiteral("%1 · %2").arg(dayNames[busiestIdx],
                                                        formatDuration(thisWeek[busiestIdx])));
        quietest_->setText(QStringLiteral("%1 · %2").arg(dayNames[quietestIdx],
                                                         formatDuration(thisWeek[quietestIdx])));
    }

    int peakHour = 0;
    for (int hh = 1; hh < 24; ++hh) {
        if (h[weekIndex][hh] > h[weekIndex][peakHour])
            peakHour = hh;
    }

    if (h[weekIndex][peakHour] <= 0) {
        peakHour_->setText(QStringLiteral("—"));
        peakDetail_->setText(QStringLiteral("No usage recorded today"));
    } else {
        peakHour_->setText(formatHour(peakHour));
        peakDetail_->setText(QStringLiteral("%1 in that hour").arg(formatDuration(h[weekIndex][peakHour])));
    }

    qint64 morning = 0, afternoon = 0, evening = 0, night = 0;
    for (int d = 0; d < daysElapsed; ++d) {
        for (int hh = 0; hh < 24; ++hh) {
            const qint64 ms = h[d][hh];
            if (hh >= 5 && hh < 12)
                morning += ms;
            else if (hh >= 12 && hh < 17)
                afternoon += ms;
            else if (hh >= 17 && hh < 22)
                evening += ms;
            else
                night += ms;
        }
    }

    const qint64 daypartTotal = morning + afternoon + evening + night;
    auto setShare = [&](QProgressBar* bar, QLabel* value, qint64 part) {
        const double share = daypartTotal <= 0 ? 0.0 : static_cast<double>(part) / daypartTotal;
        bar->setValue(static_cast<int>(share * 1000.0));
        value->setText(formatPercent(share));
    };
    setShare(morningBar_, morning_, morning);
    setShare(afternoonBar_, afternoon_, afternoon);
    setShare(eveningBar_, evening_, evening);
    setShare(nightBar_, night_, night);

    const auto apps = tracker_.store().app_breakdown(localDayString(thisMonday).toStdString(),
                                                     localDayString(today).toStdString());
    qint64 appTotal = 0;
    int active = 0;
    for (const auto& row : apps) {
        appTotal += row.duration_ms;
        if (row.duration_ms >= 60000)
            ++active;
    }
    activeApps_->setText(QString::number(active));

    if (apps.empty() || appTotal <= 0) {
        dominant_->setText(QStringLiteral("—"));
        dominantDetail_->setText(QStringLiteral("No apps tracked this week"));
    } else {
        const auto& top = apps.front();
        const double share = static_cast<double>(top.duration_ms) / appTotal;
        const QString name = QString::fromStdWString(top.display_name);
        dominant_->setText(name.trimmed().isEmpty() ? QStringLiteral("Unknown") : name);
        dominantDetail_->setText(QStringLiteral("%1 · %2 of week")
                                     .arg(formatDuration(top.duration_ms), formatPercent(share)));
    }

    QVector<ChartBarData> trend;
    qint64 trendMax = 1;
    for (int i = 0; i < 14; ++i)
        trendMax = std::max(trendMax, dayTotals[i]);

    for (int i = 0; i < 14; ++i) {
        const QDate day = today.addDays(-(13 - i));
        ChartBarData d;
        d.durationMs = dayTotals[i];
        d.tooltip = formatDuration(dayTotals[i]);
        d.label = (i == 13 || day.dayOfWeek() == Qt::Monday) ? day.toString(QStringLiteral("d/M")) : QString();
        trend.append(d);
    }
    trend_->setBars(trend);
}