#include "core/AppPaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>

#include "core/Logger.h"

namespace cf::paths {

static QString exeDir()
{
    return QCoreApplication::applicationDirPath();
}

QString portableDir()
{
    const QString candidate = exeDir() + QStringLiteral("/portable");
    if (QFileInfo::exists(candidate)) return candidate;
    return QString();
}

bool isPortable()
{
    return !portableDir().isEmpty();
}

QString dataRoot()
{
    const QString portable = portableDir();
    if (!portable.isEmpty()) {
        const QString p = portable + QStringLiteral("/data");
        QDir().mkpath(p);
        return p;
    }
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (base.isEmpty()) base = QDir::homePath() + QStringLiteral("/.codeforge");
    QDir().mkpath(base);
    return base;
}

QString settingsFile()   { return dataRoot() + QStringLiteral("/settings.json"); }
QString recentsFile()    { return dataRoot() + QStringLiteral("/recents.json"); }
QString sessionFile()    { return dataRoot() + QStringLiteral("/session.json"); }
QString themesDir()      { QDir().mkpath(dataRoot() + QStringLiteral("/themes")); return dataRoot() + QStringLiteral("/themes"); }
QString extensionsDir()  { QDir().mkpath(dataRoot() + QStringLiteral("/extensions")); return dataRoot() + QStringLiteral("/extensions"); }
QString recoveryDir()    { QDir().mkpath(dataRoot() + QStringLiteral("/recovery")); return dataRoot() + QStringLiteral("/recovery"); }
QString keysDir()        { QDir().mkpath(dataRoot() + QStringLiteral("/keys")); return dataRoot() + QStringLiteral("/keys"); }
QString logsDir()        { return dataRoot(); }

QString workspaceSettingsFile(const QString& workspaceRoot)
{
    return workspaceRoot + QStringLiteral("/.codeforge/settings.json");
}

}  // namespace cf::paths
