#pragma once

#include <QObject>
#include <QVersionNumber>

class QNetworkAccessManager;
class QNetworkReply;

// GitHub Releases updater ported from the WinUI UpdateService:
// ETag-aware checks, local cache, dismiss, download progress, silent install.
class UpdateService : public QObject {
    Q_OBJECT
public:
    enum class Status {
        UpToDate,
        Checking,
        Available,
        Downloading,
        Installing,
        Failed,
    };
    Q_ENUM(Status)

    explicit UpdateService(QObject* parent = nullptr);

    Status status() const { return status_; }
    QVersionNumber availableVersion() const { return availableVersion_; }
    QString downloadUrl() const { return downloadUrl_; }
    QString lastError() const { return lastError_; }
    int downloadProgress() const { return downloadProgress_; }

    static QVersionNumber localVersion();

public slots:
    void checkForUpdates(bool force = false);
    void downloadAndInstall();
    void dismiss();

signals:
    void stateChanged();

private:
    struct Cache {
        QString etag;
        QString lastCheckedUtc;
        QString latestVersion;
        QString downloadUrl;
        QString dismissedVersion;
    };

    void loadCache();
    void saveCache();
    void applyCache();
    void setStatus(Status s);
    void raiseStateChanged();

    void onCheckFinished(QNetworkReply* reply);
    void onDownloadProgress(qint64 received, qint64 total);
    void onDownloadFinished(QNetworkReply* reply);
    void startInstaller(const QString& setupPath);

    QNetworkAccessManager* checkManager_ = nullptr;
    QNetworkAccessManager* downloadManager_ = nullptr;
    QNetworkReply* downloadReply_ = nullptr;
    QIODevice* downloadFile_ = nullptr;
    QString tempDir_;
    QString tempSetupPath_;

    Cache cache_;
    Status status_ = Status::UpToDate;
    QVersionNumber availableVersion_;
    QString downloadUrl_;
    QString lastError_;
    int downloadProgress_ = 0;
    int lastRaisedProgressPercent_ = -1;
    bool checking_ = false;
};