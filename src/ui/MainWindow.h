#pragma once

#include "core/tracker.h"
#include "services/UpdateService.h"
#include "ui/InsightsPage.h"
#include "ui/OverviewPage.h"
#include "ui/SettingsPanel.h"
#include "services/TrayService.h"

#include <QMainWindow>
#include <QTimer>

class QLabel;
class QProgressBar;
class QPushButton;
class QStackedWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(Tracker& tracker, QWidget* parent = nullptr);
    ~MainWindow() override;
protected:
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;
private slots:
    void showOverview();
    void showInsights();
    void openSettings();
    void applySettings();
    void clearData();
    void refreshAll();
    void onTrayShow();
    void onTrayExit();
    void onUpdateStateChanged();
    void installUpdate();
    void dismissUpdate();
private:
    void buildSidebar(QWidget* sidebar);
    void setNavSelected(QPushButton* selected);
    void buildUpdateBubble(QWidget* parent);
    void refreshUpdateBubble();
    void applyUpdateBubbleTheme();
    void applySidebarTheme();
    Tracker& tracker_;
    OverviewPage* overview_ = nullptr;
    InsightsPage* insights_ = nullptr;
    SettingsPanel* settings_ = nullptr;
    QStackedWidget* stack_ = nullptr;
    QLabel* todayTotal_ = nullptr;
    QPushButton* overviewNav_ = nullptr;
    QPushButton* insightsNav_ = nullptr;
    QPushButton* settingsNav_ = nullptr;
    TrayService* tray_ = nullptr;
    UpdateService* updates_ = nullptr;
    QWidget* updateBubble_ = nullptr;
    QLabel* updateBubbleText_ = nullptr;
    QProgressBar* updateBubbleProgress_ = nullptr;
    QPushButton* updateBubbleAction_ = nullptr;
    QPushButton* updateBubbleDismiss_ = nullptr;
    QWidget* sidebar_ = nullptr;
    QTimer refreshTimer_;
    bool quitting_ = false;
};
