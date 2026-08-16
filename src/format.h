#pragma once

#include <QDate>
#include <QString>

// Shared display/format helpers (previously duplicated in MainWindow,
// OverviewPage and InsightsPage).

inline QString formatDuration(qint64 ms)
{
    if (ms < 0)
        ms = 0;
    const qint64 totalMin = ms / 60000;
    if (totalMin >= 60)
        return QStringLiteral("%1h %2m").arg(totalMin / 60).arg(totalMin % 60);
    if (totalMin >= 1)
        return QStringLiteral("%1m").arg(totalMin);
    if (ms == 0)
        return QStringLiteral("0m");
    return QStringLiteral("%1s").arg(qMax<qint64>(1, ms / 1000));
}

inline QString localDayString(const QDate& date)
{
    return date.toString(QStringLiteral("yyyy-MM-dd"));
}