#include "filesystem/FileWatcher.h"

#include <QDir>
#include <QFileInfo>

#include "core/Logger.h"

namespace cf {

FileWatcher::FileWatcher(QObject* parent) : QObject(parent)
{
    m_dirDebounce.setSingleShot(true);
    m_dirDebounce.setInterval(300);
    m_fileDebounce.setSingleShot(true);
    m_fileDebounce.setInterval(300);
    connect(&m_dirDebounce, &QTimer::timeout, this, &FileWatcher::workspaceChanged);
    connect(&m_fileDebounce, &QTimer::timeout, this, &FileWatcher::filesChangedExternally);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &FileWatcher::onDirectoryChanged);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, &FileWatcher::onFileChanged);
}

void FileWatcher::setWorkspaceRoot(const QString& root)
{
    clear();
    m_root = root;
    if (!m_root.isEmpty()) {
        m_watcher.addPath(m_root);
        addDirRecursive(m_root, 0);
        CF_LOG_INFO(QStringLiteral("FileWatcher: watching %1 (%2 dirs)").arg(m_root).arg(m_watcher.directories().size()));
    }
}

void FileWatcher::addDirRecursive(const QString& dir, int depth)
{
    // Depth budget guards pathological trees (build dirs with huge depth);
    // children are picked up when the user expands them / on rescan.
    if (depth > 6 || m_watcher.directories().size() > 4000) return;
    QDir d(dir);
    for (const QFileInfo& fi : d.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks)) {
        const QString sub = fi.absoluteFilePath();
        if (sub.contains(QLatin1String("/.git"))) continue;
        m_watcher.addPath(sub);
        addDirRecursive(sub, depth + 1);
    }
}

void FileWatcher::clear()
{
    const QStringList dirs = m_watcher.directories();
    if (!dirs.isEmpty()) m_watcher.removePaths(dirs);
    const QStringList files = m_watcher.files();
    if (!files.isEmpty()) m_watcher.removePaths(files);
    m_root.clear();
    m_watchFiles.clear();
}

void FileWatcher::setWatchedFiles(const QStringList& paths)
{
    const QStringList oldFiles = m_watcher.files();
    if (!oldFiles.isEmpty()) m_watcher.removePaths(oldFiles);
    m_watchFiles.clear();
    for (const QString& p : paths)
        if (QFileInfo::exists(p) && m_watcher.addPath(p))
            m_watchFiles.append(p);
}

void FileWatcher::onDirectoryChanged(const QString& path)
{
    Q_UNUSED(path);
    // Re-register new subdirectories created under changed dirs.
    if (!m_root.isEmpty()) {
        addDirRecursive(m_root, 0);
        m_dirDebounce.start();
    }
}

void FileWatcher::onFileChanged(const QString& path)
{
    // Files may be replaced (atomic save from other editors): re-add path.
    if (QFileInfo::exists(path) && !m_watcher.files().contains(path))
        m_watcher.addPath(path);
    if (m_watchFiles.contains(path))
        m_fileDebounce.start();
}

}  // namespace cf
