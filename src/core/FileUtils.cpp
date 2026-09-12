#include "core/FileUtils.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>

#include "core/Common.h"
#include "core/Logger.h"

namespace cf::fs {

bool readAll(const QString& path, QByteArray& out, QString* error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) *error = QStringLiteral("Cannot open file: %1 (%2)").arg(path, f.errorString());
        return false;
    }
    out = f.readAll();
    if (f.error() != QFileDevice::NoError) {
        if (error) *error = QStringLiteral("Read failed: %1 (%2)").arg(path, f.errorString());
        return false;
    }
    return true;
}

bool writeAllAtomic(const QString& path, const QByteArray& data, QString* error)
{
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        if (error)
            *error = QStringLiteral("Unable to save file.\n\nThe file may be locked by another application.\n\n%1 (%2)")
                         .arg(path, f.errorString());
        return false;
    }
    f.write(data);
    if (!f.commit()) {
        if (error) *error = QStringLiteral("Unable to save file.\n\n%1 (%2)").arg(path, f.errorString());
        return false;
    }
    return true;
}

bool removePath(const QString& path, bool permanent, QString* error)
{
    QFileInfo info(path);
    if (!info.exists()) {
        if (error) *error = QStringLiteral("Path does not exist: %1").arg(path);
        return false;
    }
    if (!permanent) {
        if (QFile::moveToTrash(path)) return true;
        CF_LOG_WARNING(QStringLiteral("moveToTrash failed, falling back to permanent delete: %1").arg(path));
    }
    if (info.isDir()) {
        QDir d(path);
        // Recursive remove; refuse on failure so caller can show error.
        if (!d.removeRecursively()) {
            if (error) *error = QStringLiteral("Failed to delete folder: %1").arg(path);
            return false;
        }
        return true;
    }
    if (!QFile::remove(path)) {
        if (error) *error = QStringLiteral("Failed to delete file (it may be locked): %1").arg(path);
        return false;
    }
    return true;
}

bool duplicateEntry(const QString& path, QString* error)
{
    QFileInfo info(path);
    const QString target = uniqueSiblingName(path, info.isDir());
    if (info.isDir())
        return QDir().rename(path, target) ? true : (error ? (*error = QStringLiteral("Failed to duplicate folder")), false : false);
    if (!QFile::copy(path, target)) {
        if (error) *error = QStringLiteral("Failed to duplicate file: %1").arg(path);
        return false;
    }
    return true;
}

QString uniqueSiblingName(const QString& path, bool isDir)
{
    QFileInfo info(path);
    const QDir dir = info.absoluteDir();
    const QString base = isDir ? info.fileName() : baseNameOf(info.fileName());
    const QString ext = isDir ? QString() : (QLatin1Char('.') + fileExtensionOf(info.fileName()));
    for (int i = 1;; ++i) {
        const QString candidate = dir.filePath(QStringLiteral("%1 - Copy%2").arg(base).arg(i == 1 ? QString() : QStringLiteral(" %1").arg(i)) + ext);
        if (!QFileInfo::exists(candidate)) return candidate;
    }
}

bool ensureParentDir(const QString& path)
{
    const QFileInfo info(path);
    return QDir().mkpath(info.absolutePath());
}

bool openTerminalAt(const QString& dir)
{
#ifdef Q_OS_WIN
    const QString shell = qEnvironmentVariable("ComSpec");
    return QProcess::startDetached(shell.isEmpty() ? QStringLiteral("cmd.exe") : shell, QStringList(), dir);
#else
    for (const QString& term : { QStringLiteral("x-terminal-emulator"), QStringLiteral("gnome-terminal"), QStringLiteral("konsole"), QStringLiteral("xterm") }) {
        if (QStandardPaths::findExecutable(term).isEmpty()) continue;
        QStringList args;
        if (term == QStringLiteral("gnome-terminal")) args << QStringLiteral("--working-directory=") + dir;
        else if (term == QStringLiteral("konsole")) args << QStringLiteral("--workdir") << dir;
        else args << QStringLiteral("-e") << QStringLiteral("bash");
        if (QProcess::startDetached(term, args, dir)) return true;
    }
    return false;
#endif
}

void revealInFileManager(const QString& path)
{
#ifdef Q_OS_WIN
    const QFileInfo info(path);
    if (info.isDir())
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    else
        QProcess::startDetached(QStringLiteral("explorer.exe"), { QStringLiteral("/select,"), QDir::toNativeSeparators(path) });
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
#endif
}

void openWithDefaultApp(const QString& path)
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

bool isTextFileByHead(const QString& path, QString* error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) *error = f.errorString();
        return false;
    }
    const QByteArray head = f.read(8192);
    f.close();
    if (cf::isProbablyBinary(head)) {
        if (error) *error = QStringLiteral("The file appears to be binary and cannot be opened as text.");
        return false;
    }
    return true;
}

}  // namespace cf::fs
