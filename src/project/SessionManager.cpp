#include "project/SessionManager.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>

#include "core/AppPaths.h"
#include "core/FileUtils.h"
#include "ui/MainWindow.h"

namespace cf {

SessionManager::SessionManager(QObject* parent) : QObject(parent) {}

void SessionManager::saveMainWindow(MainWindow* window)
{
    if (!window) return;
    const QJsonObject state = window->sessionState();
    cf::fs::ensureParentDir(paths::sessionFile());
    QFile f(paths::sessionFile());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(state).toJson(QJsonDocument::Compact));
}

bool SessionManager::hasSession() const
{
    return QFileInfo::exists(paths::sessionFile());
}

QJsonObject SessionManager::loadSession() const
{
    QFile f(paths::sessionFile());
    if (!f.open(QIODevice::ReadOnly)) return QJsonObject();
    return QJsonDocument::fromJson(f.readAll()).object();
}

void SessionManager::clear()
{
    QFile::remove(paths::sessionFile());
}

}  // namespace cf
