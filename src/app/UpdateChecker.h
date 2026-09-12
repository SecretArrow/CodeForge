#pragma once
// UpdateChecker: optional, opt-in update notification via the GitHub
// Releases API. Offline-first: nothing is requested unless the user opted in
// (startup check) or triggers a manual check. Never downloads anything.
#include <QJsonObject>
#include <QObject>

class QNetworkAccessManager;
class QNetworkReply;

namespace cf {

class UpdateChecker : public QObject {
    Q_OBJECT
public:
    explicit UpdateChecker(QObject* parent = nullptr);

    // Manual check (Help menu) — always allowed.
    void checkNow();
    // Startup check — only runs when settings updates.checkOnStartup is true.
    void maybeCheckOnStartup();

    static bool isNewerVersion(const QString& current, const QString& latest);

signals:
    void checkStarted();
    void updateAvailable(const QString& currentVersion, const QString& latestVersion, const QString& releaseUrl);
    void upToDate();
    void checkFailed(const QString& error);

private:
    QNetworkReply* fetchLatest();
    void handleReply(QNetworkReply* reply);

    QNetworkAccessManager* m_nam = nullptr;
    QJsonObject m_lastPayload;
};

}  // namespace cf
