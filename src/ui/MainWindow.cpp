#include "ui/MainWindow.h"

#include "theme.h"
#include "format.h"
#include "services/Autostart.h"
#include "services/TrayService.h"
#include "AppIcon.h"

#include <QApplication>
#include <QCloseEvent>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QSettings>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>

#include <Windows.h>
#include <dwmapi.h>

MainWindow::MainWindow(Tracker& tracker, QWidget* parent)
    : QMainWindow(parent)
    , tracker_(tracker)
{
    setWindowTitle(QStringLiteral("Screeni"));
    resize(Theme::WindowWidth, Theme::WindowHeight);
    setWindowIcon(AppIcon::load());

    // Match the native caption/title bar colour to the page background (Windows 11 22H2+).
    const auto bg = RGB(Theme::palette().pageBg.red(), Theme::palette().pageBg.green(), Theme::palette().pageBg.blue());
    QTimer::singleShot(0, this, [this, bg] {
        if (const QWindow* w = windowHandle()) {
            const auto hwnd = reinterpret_cast<HWND>(w->winId());
            DwmSetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &bg, sizeof(bg));
        }
    });

    auto* central = new QWidget(this);
    setCentralWidget(central);

    auto* root = new QHBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* sidebar = new QFrame;
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setFixedWidth(Theme::SidebarWidth);
    sidebar_ = sidebar;
    applySidebarTheme();
    buildSidebar(sidebar);
    root->addWidget(sidebar);

    stack_ = new QStackedWidget;
    root->addWidget(stack_, 1);

    overview_ = new OverviewPage(tracker_);
    insights_ = new InsightsPage(tracker_);
    stack_->addWidget(overview_);
    stack_->addWidget(insights_);

    settings_ = new SettingsPanel(central);
    settings_->setGeometry(central->rect());
    settings_->setStartWithWindows(Autostart::isEnabled());
    settings_->setIdleThresholdSec(tracker_.idle_threshold_sec());
    settings_->setTheme(Theme::currentId());
    settings_->hide();

    connect(settings_, &SettingsPanel::applyRequested, this, &MainWindow::applySettings);
    connect(settings_, &SettingsPanel::clearDataRequested, this, &MainWindow::clearData);
    connect(settings_, &SettingsPanel::closeRequested, this, [this] { settings_->hide(); });

    tray_ = new TrayService(QStringLiteral("Screeni"), this);
    connect(tray_, &TrayService::showRequested, this, &MainWindow::onTrayShow);
    connect(tray_, &TrayService::exitRequested, this, &MainWindow::onTrayExit);

    updates_ = new UpdateService(this);
    connect(updates_, &UpdateService::stateChanged, this, &MainWindow::onUpdateStateChanged);

    refreshTimer_.setInterval(15000);
    connect(&refreshTimer_, &QTimer::timeout, this, &MainWindow::refreshAll);
    refreshTimer_.start();

    // Force a check on every launch; the bubble drives install flow.
    QTimer::singleShot(1500, this, [this] { updates_->checkForUpdates(true); });

    showOverview();
}

MainWindow::~MainWindow() = default;

void MainWindow::buildSidebar(QWidget* sidebar)
{
    auto* lay = new QVBoxLayout(sidebar);
    lay->setContentsMargins(14, 31, 14, 0);
    lay->setSpacing(4);

    auto* logoCard = new QFrame;
    logoCard->setObjectName(QStringLiteral("card"));
    logoCard->setFixedHeight(39);
    logoCard->setStyleSheet(QStringLiteral("QFrame#card { background: transparent; border: none; border-radius: 10px; }"));
    auto* logoLay = new QHBoxLayout(logoCard);
    logoLay->setContentsMargins(10, 4, 10, 4);
    auto* logo = new QLabel(QStringLiteral("🌿 Screeni"));
    logo->setObjectName(QStringLiteral("section"));
    logoLay->addWidget(logo);
    lay->addWidget(logoCard);

    auto* totalCard = new QFrame;
    totalCard->setObjectName(QStringLiteral("card"));
    totalCard->setFixedHeight(137);
    totalCard->setStyleSheet(QStringLiteral("QFrame#card { background: transparent; border: none; border-radius: 10px; }"));
    auto* totalLay = new QVBoxLayout(totalCard);
    totalLay->setContentsMargins(12, 18, 12, 18);
    auto* totalCaption = new QLabel(QStringLiteral("Total Screen Time"));
    totalCaption->setObjectName(QStringLiteral("muted"));
    totalCaption->setStyleSheet(QStringLiteral("font-size: 13px;"));
    todayTotal_ = new QLabel(QStringLiteral("0m"));
    todayTotal_->setObjectName(QStringLiteral("display"));
    auto* todayCaption = new QLabel(QStringLiteral("Today"));
    todayCaption->setObjectName(QStringLiteral("muted"));
    todayCaption->setStyleSheet(QStringLiteral("font-size: 13px;"));
    totalLay->addWidget(totalCaption);
    totalLay->addWidget(todayTotal_);
    totalLay->addWidget(todayCaption);
    lay->addWidget(totalCard);

    overviewNav_ = new QPushButton(QStringLiteral("Overview"));
    insightsNav_ = new QPushButton(QStringLiteral("Insights"));
    settingsNav_ = new QPushButton(QStringLiteral("Settings"));
    overviewNav_->setObjectName(QStringLiteral("nav"));
    insightsNav_->setObjectName(QStringLiteral("nav"));
    settingsNav_->setObjectName(QStringLiteral("nav"));
    for (QPushButton* btn : {overviewNav_, insightsNav_, settingsNav_}) {
        btn->setCursor(Qt::PointingHandCursor);
        lay->addWidget(btn);
    }

    connect(overviewNav_, &QPushButton::clicked, this, &MainWindow::showOverview);
    connect(insightsNav_, &QPushButton::clicked, this, &MainWindow::showInsights);
    connect(settingsNav_, &QPushButton::clicked, this, &MainWindow::openSettings);

    lay->addStretch();
    buildUpdateBubble(sidebar);
}

void MainWindow::setNavSelected(QPushButton* selected)
{
    for (QPushButton* btn : {overviewNav_, insightsNav_, settingsNav_}) {
        btn->setProperty("navSelected", btn == selected);
        btn->style()->unpolish(btn);
        btn->style()->polish(btn);
    }
}

void MainWindow::showOverview()
{
    setNavSelected(overviewNav_);
    stack_->setCurrentWidget(overview_);
    const qint64 todayMs = tracker_.store().today_total_ms();
    todayTotal_->setText(formatDuration(todayMs));
    overview_->refresh();
}

void MainWindow::showInsights()
{
    setNavSelected(insightsNav_);
    stack_->setCurrentWidget(insights_);
    insights_->refresh();
}

void MainWindow::openSettings()
{
    settings_->setGeometry(centralWidget()->rect());
    settings_->show();
    settings_->raise();
}

void MainWindow::applySettings()
{
    tracker_.set_idle_threshold_sec(settings_->idleThresholdSec());
    Autostart::setEnabled(settings_->startWithWindows());

    const Theme::Id selected = settings_->theme();
    if (selected != Theme::currentId()) {
        Theme::setTheme(selected);
        QSettings().setValue(QStringLiteral("theme"), Theme::themeName(selected));
        qApp->setStyleSheet(Theme::globalStyleSheet());
        applyUpdateBubbleTheme();
        applySidebarTheme();
        settings_->retheme();
        refreshAll();
        if (const QWindow* w = windowHandle()) {
            const auto bg = RGB(Theme::palette().pageBg.red(), Theme::palette().pageBg.green(), Theme::palette().pageBg.blue());
            const auto hwnd = reinterpret_cast<HWND>(w->winId());
            DwmSetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &bg, sizeof(bg));
        }
    }

    settings_->hide();
}

void MainWindow::clearData()
{
    tracker_.store().clear_all();
    refreshAll();
}

void MainWindow::refreshAll()
{
    const qint64 todayMs = tracker_.store().today_total_ms();
    const QString tip = QStringLiteral("Screeni — %1").arg(formatDuration(todayMs));
    if (tip != trayTip_) {
        tray_->updateTip(tip);
        trayTip_ = tip;
    }

    // The pages rebuild widgets and hit the shell for icons; with the window in
    // the tray (or the page off-screen) that work produces nothing visible.
    // Hidden pages refresh lazily when navigated to (showOverview/showInsights).
    if (!isVisible())
        return;

    todayTotal_->setText(formatDuration(todayMs));
    if (stack_->currentWidget() == overview_)
        overview_->refresh();
    else
        insights_->refresh();
}

void MainWindow::onTrayShow()
{
    show();
    raise();
    activateWindow();
    refreshAll();
}

void MainWindow::onTrayExit()
{
    quitting_ = true;
    close();
    qApp->quit();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (quitting_) {
        event->accept();
        return;
    }
    hide();
    event->ignore();
}

void MainWindow::changeEvent(QEvent* event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange && !isMinimized()) {
        refreshAll();
    }
}

void MainWindow::buildUpdateBubble(QWidget* sidebar)
{
    auto* lay = sidebar->layout();

    updateBubble_ = new QFrame(sidebar);
    updateBubble_->setObjectName(QStringLiteral("updateBubble"));
    applyUpdateBubbleTheme();
    auto* bubbleLay = new QVBoxLayout(updateBubble_);
    bubbleLay->setContentsMargins(10, 8, 10, 10);
    bubbleLay->setSpacing(6);

    auto* headRow = new QHBoxLayout;
    headRow->setSpacing(6);
    auto* title = new QLabel(QStringLiteral("Update available"));
    title->setObjectName(QStringLiteral("section"));
    title->setStyleSheet(QStringLiteral("font-size: 13px;"));
    title->setWordWrap(true);
    title->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    headRow->addWidget(title, 1);
    auto* dismiss = new QPushButton(QStringLiteral("✕"));
    dismiss->setObjectName(QStringLiteral("bubbleClose"));
    dismiss->setFixedSize(18, 18);
    dismiss->setCursor(Qt::PointingHandCursor);
    updateBubbleDismiss_ = dismiss;
    connect(dismiss, &QPushButton::clicked, this, &MainWindow::dismissUpdate);
    headRow->addWidget(dismiss, 0, Qt::AlignTop | Qt::AlignRight);
    bubbleLay->addLayout(headRow);

    updateBubbleText_ = new QLabel(QStringLiteral("v0.0.0"));
    updateBubbleText_->setObjectName(QStringLiteral("muted"));
    updateBubbleText_->setStyleSheet(QStringLiteral("font-size: 12px;"));
    updateBubbleText_->setWordWrap(true);
    bubbleLay->addWidget(updateBubbleText_);

    updateBubbleProgress_ = new QProgressBar;
    updateBubbleProgress_->setTextVisible(false);
    updateBubbleProgress_->setRange(0, 100);
    updateBubbleProgress_->setValue(0);
    updateBubbleProgress_->hide();
    bubbleLay->addWidget(updateBubbleProgress_);

    updateBubbleAction_ = new QPushButton(QStringLiteral("Install update"));
    updateBubbleAction_->setCursor(Qt::PointingHandCursor);
    connect(updateBubbleAction_, &QPushButton::clicked, this, &MainWindow::installUpdate);
    bubbleLay->addWidget(updateBubbleAction_);

    // The earlier applyUpdateBubbleTheme() call ran before the dismiss button existed,
    // so re-apply now that every child is created. Without this, the dismiss falls back
    // to the global QPushButton min-height (28px) and renders taller than it is wide.
    applyUpdateBubbleTheme();

    updateBubble_->hide();
    lay->addWidget(updateBubble_);
}

void MainWindow::applyUpdateBubbleTheme()
{
    const auto& P = Theme::palette();
    if (!updateBubble_)
        return;
    updateBubble_->setStyleSheet(QStringLiteral(
        "QFrame#updateBubble { background:%1; border:1px solid %2; border-radius:10px; }")
                                    .arg(P.surface.name(), P.borderStrong.name()));
    if (updateBubbleDismiss_) {
        updateBubbleDismiss_->setStyleSheet(QStringLiteral(
            "QPushButton#bubbleClose { background: transparent; border: none; outline: none; color:%1;"
            " font-size: 13px; font-weight: bold; padding: 0; margin: 0; min-width: 0; min-height: 0;"
            " text-align: center; }"
            "QPushButton#bubbleClose:hover { color:%2; background: transparent; border: none; }"
            "QPushButton#bubbleClose:pressed { color:%2; background: transparent; border: none; }")
                                               .arg(P.inkMuted.name(), P.ink.name()));
    }
}

void MainWindow::applySidebarTheme()
{
    if (!sidebar_)
        return;
    const auto& P = Theme::palette();
    sidebar_->setStyleSheet(QStringLiteral(
        "QFrame#sidebar { background-color:%1; border-right:1px solid %2; }")
                                .arg(P.sidebarBg.name(), P.borderSoft.name()));
}

void MainWindow::onUpdateStateChanged()
{
    refreshUpdateBubble();
}

void MainWindow::refreshUpdateBubble()
{
    if (!updateBubble_ || !updates_)
        return;

    const auto status = updates_->status();
    const bool hasUpdate = !updates_->availableVersion().isNull();
    const bool show = hasUpdate ||
                      status == UpdateService::Status::Downloading ||
                      status == UpdateService::Status::Installing;
    updateBubble_->setVisible(show);
    if (!show)
        return;

    if (!updates_->availableVersion().isNull()) {
        updateBubbleText_->setText(QStringLiteral("v%1").arg(updates_->availableVersion().toString()));
    }

    updateBubbleProgress_->setVisible(status == UpdateService::Status::Downloading ||
                                      status == UpdateService::Status::Installing);
    updateBubbleProgress_->setValue(updates_->downloadProgress());

    const bool busy = status == UpdateService::Status::Downloading ||
                      status == UpdateService::Status::Installing;
    updateBubbleAction_->setEnabled(status == UpdateService::Status::Available);
    if (status == UpdateService::Status::Downloading)
        updateBubbleAction_->setText(QStringLiteral("Downloading %1%").arg(updates_->downloadProgress()));
    else if (status == UpdateService::Status::Installing)
        updateBubbleAction_->setText(QStringLiteral("Installing..."));
    else
        updateBubbleAction_->setText(QStringLiteral("Install update"));
}

void MainWindow::installUpdate()
{
    if (updates_)
        updates_->downloadAndInstall();
}

void MainWindow::dismissUpdate()
{
    if (updates_)
        updates_->dismiss();
}
