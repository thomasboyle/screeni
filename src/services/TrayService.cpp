#include "TrayService.h"

#include "AppIcon.h"

#include <QMenu>

TrayService::TrayService(const QString& tip, QObject* parent) : QObject(parent)
{
    tray_ = new QSystemTrayIcon(this);
    tray_->setIcon(AppIcon::load());
    tray_->setToolTip(tip);

    auto* menu = new QMenu;
    auto* openAct = menu->addAction(QStringLiteral("Open Screeni"));
    auto* exitAct = menu->addAction(QStringLiteral("Exit"));
    connect(openAct, &QAction::triggered, this, &TrayService::showRequested);
    connect(exitAct, &QAction::triggered, this, &TrayService::exitRequested);
    tray_->setContextMenu(menu);

    connect(tray_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::DoubleClick || r == QSystemTrayIcon::Trigger)
            emit showRequested();
    });

    tray_->show();
}

void TrayService::updateTip(const QString& tip)
{
    tray_->setToolTip(tip);
}
