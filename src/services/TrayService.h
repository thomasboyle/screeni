#pragma once

#include <QObject>
#include <QSystemTrayIcon>

class TrayService : public QObject {
    Q_OBJECT
public:
    explicit TrayService(const QString& tip, QObject* parent = nullptr);
    void updateTip(const QString& tip);
signals:
    void showRequested();
    void exitRequested();
private:
    QSystemTrayIcon* tray_ = nullptr;
};
