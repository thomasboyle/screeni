#include "services/UpdateService.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>

#include <algorithm>

namespace {

constexpr const char* kGitHubOwner = "thomasboyle";
constexpr const char* kGitHubRepo = "screeni";
constexpr qint64 kMinRecheckMs = 6LL * 60 * 60 * 1000;  // 6 hours
constexpr int kApiTimeoutMs = 20000;
constexpr qint64 kDownloadTimeoutMs = 30LL * 60 * 1000;

QString cachePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
           QStringLiteral("/update-cache.json");
}

QVersionNumber parseTagVersion(const QString& tag)
{
    QString t = tag.trimmed();
    if (t.startsWith(QLatin1Char('v')) || t.startsWith(QLatin1Char('V')))
        t = t.mid(1);
    const QVersionNumber v = QVersionNumber::fromString(t);
    return v.isNull() ? QVersionNumber() : v;
}

}  // namespace

UpdateService::UpdateService(QObject* parent)
    : QObject(parent)
    , manager_(new QNetworkAccessManager(this))
{
    QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    loadCache();
    applyCache();
}

QVersionNumber UpdateService::localVersion()
{
    // Injected by CMake: SCREENI_VERSION=1.2.3
#ifdef SCREENI_VERSION
    return QVersionNumber::fromString(QStringLiteral(SCREENI_VERSION));
#else
    return QVersionNumber(0, 0, 0);
#endif
}

void UpdateService::checkForUpdates(bool force)
{
    if (checking_ || status_ == Status::Downloading || status_ == Status::Installing)
        return;

    const QDateTime now = QDateTime::currentDateTimeUtc();
    if (!force && !cache_.lastCheckedUtc.isEmpty()) {
        const QDateTime last = QDateTime::fromString(cache_.lastCheckedUtc, Qt::ISODateWithMs);
        if (last.isValid() && last.msecsTo(now) < kMinRecheckMs)
            return;
    }

    checking_ = true;
    setStatus(Status::Checking);
    raiseStateChanged();

    const QString apiUrl = QStringLiteral(
        "https://api.github.com/repos/%1/%2/releases/latest").arg(kGitHubOwner, kGitHubRepo);
    QUrl url(apiUrl);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Screeni-Updater"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/vnd.github+json"));
    if (!cache_.etag.isEmpty())
        req.setRawHeader("If-None-Match", cache_.etag.toUtf8());

    QNetworkReply* reply = manager_->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        onCheckFinished(reply);
        reply->deleteLater();
    });

    // Guard with a QPointer: the reply is deleteLater()'d when it finishes (the
    // GitHub call normally returns in well under 20s), so the raw pointer would
    // dangle by the time this timeout fires, crashing on reply->isFinished().
    QPointer<QNetworkReply> replyGuard(reply);
    QTimer::singleShot(kApiTimeoutMs, this, [this, replyGuard] {
        if (replyGuard && !replyGuard->isFinished()) {
            replyGuard->abort();
        }
    });
}

void UpdateService::onCheckFinished(QNetworkReply* reply)
{
    checking_ = false;

    if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 304) {
        // Not modified: keep cached version, refresh last-checked timestamp.
        const QDateTime now = QDateTime::currentDateTimeUtc();
        cache_.lastCheckedUtc = now.toString(Qt::ISODateWithMs);
        lastError_.clear();
        saveCache();
        applyCache();
        raiseStateChanged();
        return;
    }

    if (reply->error() == QNetworkReply::NoError) {
        const QByteArray data = reply->readAll();
        QJsonParseError err{};
        const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            lastError_ = QStringLiteral("Could not parse GitHub release response.");
            setStatus(status_ == Status::Available ? Status::Available : Status::Failed);
            raiseStateChanged();
            return;
        }

        const QJsonObject release = doc.object();
        const QVersionNumber remote = parseTagVersion(release.value(QStringLiteral("tag_name")).toString());
        if (remote.isNull()) {
            lastError_ = QStringLiteral("Unrecognized release tag.");
            setStatus(Status::Available == status_ ? Status::Available : Status::Failed);
            raiseStateChanged();
            return;
        }

        // Screeni ships versioned installer names: ScreeniSetup-{version}.exe
        const QString expected = QStringLiteral("ScreeniSetup-%1.exe").arg(remote.toString());
        QString downloadUrl;
        const QJsonArray assets = release.value(QStringLiteral("assets")).toArray();
        for (const auto& asset : assets) {
            const QJsonObject obj = asset.toObject();
            if (obj.value(QStringLiteral("name")).toString().compare(expected, Qt::CaseInsensitive) == 0) {
                downloadUrl = obj.value(QStringLiteral("browser_download_url")).toString();
                if (!downloadUrl.isEmpty())
                    break;
            }
        }
        if (downloadUrl.isEmpty()) {
            downloadUrl = QStringLiteral("https://github.com/%1/%2/releases/latest/download/%3")
                              .arg(kGitHubOwner, kGitHubRepo, expected);
        }

        const QDateTime now = QDateTime::currentDateTimeUtc();
        cache_.etag = reply->rawHeader("ETag");
        cache_.lastCheckedUtc = now.toString(Qt::ISODateWithMs);
        cache_.latestVersion = remote.toString();
        cache_.downloadUrl = downloadUrl;
        lastError_.clear();
        saveCache();
        applyCache();
        raiseStateChanged();
        return;
    }

    if (reply->error() == QNetworkReply::OperationCanceledError &&
        !reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).isValid()) {
        // Timeout (via abort above) or cancellation.
        if (availableVersion_.isNull()) {
            lastError_ = QStringLiteral("Update check timed out or was cancelled.");
            setStatus(Status::Failed);
        }
        raiseStateChanged();
        return;
    }

    lastError_ = reply->errorString();
    setStatus(availableVersion_.isNull() ? Status::Failed : Status::Available);
    raiseStateChanged();
}

void UpdateService::downloadAndInstall()
{
    if (availableVersion_.isNull())
        return;
    if (status_ == Status::Downloading || status_ == Status::Installing)
        return;

    tempDir_ = QDir::tempPath() + QStringLiteral("/ScreeniUpdate");
    QDir().mkpath(tempDir_);
    tempSetupPath_ = tempDir_ + QStringLiteral("/ScreeniSetup-%1.exe").arg(availableVersion_.toString());

    setStatus(Status::Downloading);
    downloadProgress_ = 0;
    lastRaisedProgressPercent_ = -1;
    raiseStateChanged();

    QUrl downloadUrl(downloadUrl_);
    QNetworkRequest req(downloadUrl);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Screeni-Updater"));
    downloadReply_ = manager_->get(req);

    downloadFile_ = new QFile(tempSetupPath_, this);
    if (!downloadFile_->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        lastError_ = QStringLiteral("Could not create update file.");
        setStatus(Status::Failed);
        raiseStateChanged();
        downloadReply_->abort();
        return;
    }

    connect(downloadReply_, &QNetworkReply::downloadProgress, this, &UpdateService::onDownloadProgress);
    connect(downloadReply_, &QNetworkReply::readyRead, this, [this] {
        if (downloadFile_ && downloadReply_)
            downloadFile_->write(downloadReply_->readAll());
    });
    connect(downloadReply_, &QNetworkReply::finished, this, [this] {
        onDownloadFinished(downloadReply_);
        downloadReply_->deleteLater();
        downloadReply_ = nullptr;
    });

    QTimer::singleShot(kDownloadTimeoutMs, this, [this] {
        if (downloadReply_ && !downloadReply_->isFinished())
            downloadReply_->abort();
    });
}

void UpdateService::onDownloadProgress(qint64 received, qint64 total)
{
    if (total <= 0) {
        return;
    }
    const int pct = static_cast<int>(std::clamp(100.0 * received / total, 0.0, 100.0));
    if (pct != lastRaisedProgressPercent_) {
        lastRaisedProgressPercent_ = pct;
        downloadProgress_ = pct;
        raiseStateChanged();
    }
}

void UpdateService::onDownloadFinished(QNetworkReply* reply)
{
    if (downloadFile_) {
        downloadFile_->close();
        downloadFile_->deleteLater();
        downloadFile_ = nullptr;
    }

    if (reply->error() != QNetworkReply::NoError) {
        if (reply->error() == QNetworkReply::OperationCanceledError &&
            !reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).isValid()) {
            setStatus(Status::Available);
            downloadProgress_ = 0;
            raiseStateChanged();
            return;
        }
        lastError_ = reply->errorString();
        setStatus(Status::Failed);
        downloadProgress_ = 0;
        raiseStateChanged();
        return;
    }

    setStatus(Status::Installing);
    downloadProgress_ = 100;
    raiseStateChanged();
    startInstaller(tempSetupPath_);
}

void UpdateService::startInstaller(const QString& setupPath)
{
    QProcess::startDetached(setupPath, {
        QStringLiteral("/VERYSILENT"),
        QStringLiteral("/CLOSEAPPLICATIONS"),
        QStringLiteral("/NORESTART"),
        QStringLiteral("/SUPPRESSMSGBOXES"),
        QStringLiteral("/SP-"),
    });
}

void UpdateService::dismiss()
{
    if (availableVersion_.isNull())
        return;

    cache_.dismissedVersion = availableVersion_.toString();
    saveCache();
    availableVersion_ = QVersionNumber();
    setStatus(Status::UpToDate);
    raiseStateChanged();
}

void UpdateService::applyCache()
{
    const QVersionNumber local = localVersion();
    const QVersionNumber remote = parseTagVersion(cache_.latestVersion);
    const QVersionNumber dismissed = parseTagVersion(cache_.dismissedVersion);
    if (remote.isNull() || cache_.downloadUrl.isEmpty() || remote <= local ||
        (!dismissed.isNull() && dismissed >= remote)) {
        availableVersion_ = QVersionNumber();
        setStatus(Status::UpToDate);
        return;
    }

    availableVersion_ = remote;
    downloadUrl_ = cache_.downloadUrl;
    setStatus(Status::Available);
}

void UpdateService::setStatus(Status s)
{
    if (status_ != s) {
        status_ = s;
    }
}

void UpdateService::raiseStateChanged()
{
    emit stateChanged();
}

void UpdateService::loadCache()
{
    QFile f(cachePath());
    if (!f.open(QIODevice::ReadOnly)) {
        return;
    }
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return;
    }
    const QJsonObject obj = doc.object();
    cache_.etag = obj.value(QStringLiteral("ETag")).toString();
    cache_.lastCheckedUtc = obj.value(QStringLiteral("LastCheckedUtc")).toString();
    cache_.latestVersion = obj.value(QStringLiteral("LatestVersion")).toString();
    cache_.downloadUrl = obj.value(QStringLiteral("DownloadUrl")).toString();
    cache_.dismissedVersion = obj.value(QStringLiteral("DismissedVersion")).toString();
}

void UpdateService::saveCache()
{
    QJsonObject obj;
    obj.insert(QStringLiteral("ETag"), cache_.etag);
    obj.insert(QStringLiteral("LastCheckedUtc"), cache_.lastCheckedUtc);
    obj.insert(QStringLiteral("LatestVersion"), cache_.latestVersion);
    obj.insert(QStringLiteral("DownloadUrl"), cache_.downloadUrl);
    obj.insert(QStringLiteral("DismissedVersion"), cache_.dismissedVersion);

    QFile f(cachePath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    }
}