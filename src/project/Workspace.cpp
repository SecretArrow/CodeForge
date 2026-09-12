#include "project/Workspace.h"

#include <QDir>

#include "core/Logger.h"
#include "project/WorkspaceIndexWorker.h"
#include "settings/SettingsManager.h"

namespace cf {

// ---------- Workspace ----------

Workspace::Workspace(QObject* parent) : QObject(parent)
{
    qRegisterMetaType<QStringList>("QStringList");
    m_worker = new WorkspaceIndexWorker();
    m_worker->moveToThread(&m_workerThread);
    connect(&m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &WorkspaceIndexWorker::done, this, [this](const QStringList& files) {
        m_files = files;
        m_files.sort(Qt::CaseInsensitive);
        m_indexReady = true;
        emit indexReady();
    }, Qt::QueuedConnection);
    m_workerThread.start();
}

Workspace::~Workspace()
{
    m_workerThread.quit();
    m_workerThread.wait(3000);
}

void Workspace::openRoot(const QString& dir)
{
    const QString newRoot = dir.isEmpty() ? QString() : QDir::fromNativeSeparators(QDir(dir).absolutePath());
    if (m_root == newRoot) return;
    m_root = newRoot;
    m_indexReady = false;
    m_files.clear();

    if (!m_root.isEmpty()) {
        applyWorkspaceSettings();
        startIndexing();
        CF_LOG_INFO(QStringLiteral("Workspace: opened %1").arg(m_root));
    } else {
        CF_LOG_INFO(QStringLiteral("Workspace: closed"));
    }
    emit rootChanged(m_root);
}

void Workspace::applyWorkspaceSettings()
{
    if (m_root.isEmpty()) return;
    SettingsManager::instance().setWorkspaceRoot(m_root);
    SettingsManager::instance().load();
}

void Workspace::startIndexing()
{
    if (m_root.isEmpty()) return;
    m_worker->root = m_root;
    m_worker->excludes = SettingsManager::instance().get(QStringLiteral("search.excludeGlobs")).toStringList();
    m_worker->includeHidden = SettingsManager::instance().getBool(QStringLiteral("search.includeHidden"));
    m_worker->limit = SettingsManager::instance().getInt(QStringLiteral("performance.fileIndexLimit"));
    QMetaObject::invokeMethod(m_worker, "run", Qt::QueuedConnection);
}

QString Workspace::relativePath(const QString& absPath) const
{
    if (m_root.isEmpty()) return absPath;
    const QString a = QDir::fromNativeSeparators(absPath);
    if (!a.startsWith(m_root)) return a;
    QString rel = a.mid(m_root.size());
    if (rel.startsWith(QLatin1Char('/'))) rel.remove(0, 1);
    return rel;
}

QString Workspace::absolutePath(const QString& relPath) const
{
    if (m_root.isEmpty()) return relPath;
    if (QDir::isAbsolutePath(relPath)) return QDir::fromNativeSeparators(relPath);
    return m_root + QLatin1Char('/') + relPath;
}

QStringList Workspace::indexedFiles() const
{
    return m_files;
}

bool Workspace::isIndexReady() const
{
    return m_indexReady;
}

bool Workspace::matchesExcludes(const QString& relPath) const
{
    const QStringList globs = SettingsManager::instance().get(QStringLiteral("search.excludeGlobs")).toStringList();
    const QString name = QFileInfo(relPath).fileName();
    for (const QString& g : globs)
        if (QDir::match(g, relPath) || QDir::match(g, name)) return true;
    return false;
}

}  // namespace cf
