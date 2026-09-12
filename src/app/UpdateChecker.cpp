#include "app/UpdateChecker.h"

#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

#include "core/Logger.h"
#include "settings/SettingsManager.h"

namespace cf {

static const char* kReleasesUrl = "https://api.github.com/repos/SecretArrow/CodeForge/releases/latest";

UpdateChecker::UpdateChecker(QObject* parent)
    : QObject(parent), m_nam(new QNetworkAccessManager(this))
{
    m_nam->setTransferTimeout(15000);
}

QNetworkReply* UpdateChecker::fetchLatest()
{
    QNetworkRequest req(QUrl(QString::fromLatin1(kReleasesUrl)));
    req.setRawHeader(QByteArray("Accept"), QByteArray("application/vnd.github+json"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    return m_nam->get(req);
}

void UpdateChecker::checkNow()
{
    emit checkStarted();
    QNetworkReply* reply = fetchLatest();
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { handleReply(reply); });
}

void UpdateChecker::maybeCheckOnStartup()
{
    if (!SettingsManager::instance().getBool(QStringLiteral("updates.checkOnStartup"))) return;
    checkNow();
}

void UpdateChecker::handleReply(QNetworkReply* reply)
{
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        CF_LOG_INFO(QStringLiteral("Update check skipped: %1").arg(reply->errorString()));
        emit checkFailed(reply->errorString());
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (!doc.isObject()) {
        emit checkFailed(QStringLiteral("Unexpected response"));
        return;
    }
    const QJsonObject obj = doc.object();
    const QString tag = obj.value(QStringLiteral("tag_name")).toString();
    const QString url = obj.value(QStringLiteral("html_url")).toString();
    if (tag.isEmpty()) {
        emit checkFailed(QStringLiteral("No release tag in response"));
        return;
    }
    const QString latest = tag.startsWith(QLatin1Char('v')) ? tag.mid(1) : tag;
    const QString current = QStringLiteral(CF_APP_VERSION);
    if (isNewerVersion(current, latest))
        emit updateAvailable(current, latest, url);
    else
        emit upToDate();
}

bool UpdateChecker::isNewerVersion(const QString& current, const QString& latest)
{
    const auto parse = [](const QString& v) -> QList<int> {
        QList<int> parts;
        for (const QString& p : v.split(QLatin1Char('.')))
            parts << p.toInt();
        while (parts.size() < 3) parts << 0;
        return parts;
    };
    const QList<int> a = parse(current);
    const QList<int> b = parse(latest);
    for (int i = 0; i < 3; ++i) {
        if (b.at(i) > a.at(i)) return true;
        if (b.at(i) < a.at(i)) return false;
    }
    return false;
}

}  // namespace cf
