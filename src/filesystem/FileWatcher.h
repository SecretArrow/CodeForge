#pragma once
// Filesystem watcher wrapper: directory changes (workspace tree) and open-file
// changes (external modification detection), both debounced.
#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QStringList>
#include <QTimer>

namespace cf {

class FileWatcher : public QObject {
    Q_OBJECT
public:
    explicit FileWatcher(QObject* parent = nullptr);

    // Watch the workspace tree (dirs only, added lazily as they appear).
    void setWorkspaceRoot(const QString& root);
    void clear();

    // Track paths of open documents for external-change checks.
    void setWatchedFiles(const QStringList& paths);

signals:
    // Emitted (debounced) when something under the workspace changed.
    void workspaceChanged();
    // Emitted (debounced) when a watched file changed on disk.
    void filesChangedExternally();

private slots:
    void onDirectoryChanged(const QString& path);
    void onFileChanged(const QString& path);

private:
    void rescanTree();
    void addDirRecursive(const QString& dir, int depth);

    QFileSystemWatcher m_watcher;
    QTimer m_dirDebounce;
    QTimer m_fileDebounce;
    QString m_root;
    QStringList m_watchFiles;
    int m_depthBudget = 0;
};

}  // namespace cf
