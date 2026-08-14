#include "ui/OverviewPage.h"

#include "theme.h"

#include <QDate>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

#include <Windows.h>
#include <psapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <tlhelp32.h>

#include <filesystem>

namespace {

QString localDayString(const QDate& date)
{
    return date.toString(QStringLiteral("yyyy-MM-dd"));
}

// Apps that self-update (e.g. Discord, Spotify) move to a fresh per-version folder over
// time, leaving the stored path stale. Match the executable's base name against running
// processes and prefer the live image path so the icon still resolves.
std::wstring liveProcessPath(const std::wstring& baseName)
{
    if (baseName.empty())
        return {};

    const HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return {};

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    std::wstring result;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, baseName.c_str()) == 0) {
                const HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
                if (proc) {
                    DWORD size = 32768;
                    std::wstring path(size, L'\0');
                    if (QueryFullProcessImageNameW(proc, 0, path.data(), &size)) {
                        path.resize(size);
                        result = path;
                    }
                    CloseHandle(proc);
                    if (!result.empty())
                        break;
                }
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return result;
}

QIcon appIconForPath(const QString& exePath)
{
    if (exePath.isEmpty())
        return {};

    const std::wstring stored = exePath.toStdWString();
    std::wstring path = stored;
    if (!std::filesystem::exists(path)) {
        const std::wstring base = std::filesystem::path(stored).filename().wstring();
        const std::wstring live = liveProcessPath(base);
        if (!live.empty())
            path = live;
    }

    SHFILEINFOW info{};
    const DWORD_PTR res = SHGetFileInfoW(path.c_str(), 0, &info, sizeof(info),
                                         SHGFI_ICON | SHGFI_LARGEICON);
    if (res == 0 || !info.hIcon)
        return {};
    QIcon icon(QPixmap::fromImage(QImage::fromHICON(info.hIcon)));
    DestroyIcon(info.hIcon);
    return icon;
}

QString monthDayLabel(const QDate& date)
{
    return date.toString(QStringLiteral("MMM d"));
}

}  // namespace

QString OverviewPage::formatDuration(qint64 ms)
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

OverviewPage::OverviewPage(Tracker& tracker, QWidget* parent)
    : QWidget(parent)
    , tracker_(tracker)
{
    setStyleSheet(QStringLiteral("background: transparent;"));

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(40, 34, 40, 0);
    lay->setSpacing(15);

    auto* title = new QLabel(QStringLiteral("Overview"));
    title->setObjectName(QStringLiteral("title"));
    lay->addWidget(title);

    auto* rangeRow = new QHBoxLayout;
    rangeRow->setContentsMargins(0, 0, 0, 0);
    dayBtn_ = new QPushButton(QStringLiteral("Day"));
    weekBtn_ = new QPushButton(QStringLiteral("Week"));
    dayBtn_->setObjectName(QStringLiteral("toggle"));
    weekBtn_->setObjectName(QStringLiteral("toggle"));
    dayBtn_->setCursor(Qt::PointingHandCursor);
    weekBtn_->setCursor(Qt::PointingHandCursor);
    rangeRow->addWidget(dayBtn_);
    rangeRow->addWidget(weekBtn_);
    rangeRow->addStretch();
    rangeLabel_ = new QLabel(QStringLiteral("Today"));
    rangeLabel_->setObjectName(QStringLiteral("muted"));
    rangeRow->addWidget(rangeLabel_);
    lay->addLayout(rangeRow);

    auto* chartCard = new QFrame;
    chartCard->setObjectName(QStringLiteral("card"));
    auto* chartLay = new QVBoxLayout(chartCard);
    chartLay->setContentsMargins(28, 18, 28, 18);
    auto* chartTitle = new QLabel(QStringLiteral("Screen Time"));
    chartTitle->setObjectName(QStringLiteral("section"));
    chartLay->addWidget(chartTitle);
    chart_ = new BarChart;
    chart_->setMinimumHeight(190);
    chartLay->addWidget(chart_);
    lay->addWidget(chartCard);

    auto* appsCard = new QFrame;
    appsCard->setObjectName(QStringLiteral("card"));
    auto* appsLay = new QVBoxLayout(appsCard);
    appsLay->setContentsMargins(0, 0, 0, 0);
    auto* appsTitle = new QLabel(QStringLiteral("Top Apps"));
    appsTitle->setObjectName(QStringLiteral("section"));
    appsTitle->setContentsMargins(28, 18, 28, 18);
    appsLay->addWidget(appsTitle);
    appsList_ = new QListWidget;
    appsList_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    appsLay->addWidget(appsList_);
    lay->addWidget(appsCard, 1);

    connect(dayBtn_, &QPushButton::clicked, this, &OverviewPage::dayRangeSelected);
    connect(weekBtn_, &QPushButton::clicked, this, &OverviewPage::weekRangeSelected);
    connect(dayBtn_, &QPushButton::clicked, this, [this] { setRange(Range::Day); });
    connect(weekBtn_, &QPushButton::clicked, this, [this] { setRange(Range::Week); });

    refresh();
}

void OverviewPage::refresh()
{
    const QDate today = QDate::currentDate();
    QString rangeText;
    QVector<ChartBarData> bars;
    QListWidgetItem* appItems[5] = {};
    auto addBar = [&bars](const QString& label, qint64 ms) {
        ChartBarData d;
        d.label = label;
        d.durationMs = ms;
        d.tooltip = formatDuration(ms);
        bars.append(d);
    };

    if (range_ == Range::Day) {
        rangeText = QStringLiteral("Today");
        const auto buckets = tracker_.store().hourly_totals(localDayString(today).toStdString());
        for (int h = 0; h < 24; ++h)
            addBar(h % 6 == 0 ? QString::number(h) : QString(), buckets[h]);
        chart_->setBarWidth(13);
    } else {
        const int delta = (today.dayOfWeek() + 6) % 7;  // Mon=0 … Sun=6
        const QDate start = today.addDays(-delta);
        rangeText = QStringLiteral("%1 – %2").arg(monthDayLabel(start), monthDayLabel(start.addDays(6)));
        const auto buckets = tracker_.store().week_day_totals(localDayString(start).toStdString());
        const QStringList days = {QStringLiteral("Mon"), QStringLiteral("Tue"), QStringLiteral("Wed"),
                                  QStringLiteral("Thu"), QStringLiteral("Fri"), QStringLiteral("Sat"),
                                  QStringLiteral("Sun")};
        for (int i = 0; i < 7; ++i)
            addBar(days[i], buckets[i]);
        chart_->setBarWidth(52);
    }

    rangeLabel_->setText(rangeText);
    chart_->setBars(bars);

    appsList_->clear();
    const auto rows = range_ == Range::Day
        ? tracker_.store().app_breakdown(localDayString(today).toStdString(), localDayString(today).toStdString())
        : tracker_.store().app_breakdown(localDayString(today.addDays(-((today.dayOfWeek() + 6) % 7))).toStdString(),
                                         localDayString(today.addDays(-((today.dayOfWeek() + 6) % 7) + 6)).toStdString());

    qint64 total = 0;
    for (const auto& row : rows)
        total += row.duration_ms;

    int shown = 0;
    for (const auto& row : rows) {
        if (shown >= 5)
            break;
        const QString name = QString::fromStdWString(row.display_name);
        const QString exe = QString::fromStdWString(row.exe_path);
        const QString initial = name.trimmed().isEmpty() ? QStringLiteral("?")
                                                         : name.trimmed().at(0).toUpper();

        auto* item = new QListWidgetItem(appsList_);
        item->setSizeHint(QSize(0, 64));

        auto* cell = new QWidget;
        auto* rowLay = new QHBoxLayout(cell);
        rowLay->setContentsMargins(28, 0, 28, 0);
        rowLay->setSpacing(16);

        auto* icon = new QLabel;
        icon->setFixedSize(40, 40);
        icon->setAlignment(Qt::AlignCenter);
        icon->setStyleSheet(QStringLiteral(
            "QLabel { background:#F7F0E2; border:1px solid #A7A28B; border-radius:8px; }"));
        const QIcon appIcon = appIconForPath(exe);
        if (!appIcon.isNull()) {
            icon->setPixmap(appIcon.pixmap(36, 36));
        } else {
            icon->setText(initial);
            icon->setStyleSheet(QStringLiteral(
                "QLabel { background:#F7F0E2; border:1px solid #A7A28B; border-radius:8px; font-size:16px; }"));
        }
        rowLay->addWidget(icon);

        auto* nameLabel = new QLabel;
        nameLabel->setFixedWidth(200);
        nameLabel->setStyleSheet(QStringLiteral("font-size: 14px;"));
        nameLabel->setToolTip(exe);
        const QFontMetrics fm(nameLabel->font());
        nameLabel->setText(fm.elidedText(name, Qt::ElideRight, 200));
        rowLay->addWidget(nameLabel);

        auto* bar = new QProgressBar;
        bar->setTextVisible(false);
        bar->setRange(0, 1000);
        bar->setValue(total <= 0 ? 0 : static_cast<int>((row.duration_ms * 1000.0) / total));
        rowLay->addWidget(bar, 1);

        auto* durLabel = new QLabel(formatDuration(row.duration_ms));
        durLabel->setObjectName(QStringLiteral("muted"));
        durLabel->setStyleSheet(QStringLiteral("font-size: 14px;"));
        durLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        durLabel->setFixedWidth(80);
        rowLay->addWidget(durLabel);

        appsList_->setItemWidget(item, cell);
        ++shown;
    }
}

void OverviewPage::setRange(Range r)
{
    range_ = r;
    for (QPushButton* btn : {dayBtn_, weekBtn_})
        btn->setObjectName(QStringLiteral("toggle"));
    QPushButton* selected = range_ == Range::Day ? dayBtn_ : weekBtn_;
    selected->setObjectName(QStringLiteral("toggleChecked"));
    for (QPushButton* btn : {dayBtn_, weekBtn_}) {
        btn->style()->unpolish(btn);
        btn->style()->polish(btn);
    }
    refresh();
}