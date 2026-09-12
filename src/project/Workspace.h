#pragma once
// Workspace: the open root folder. Maintains an async file index for Quick
// Open / Search, applies exclude globs + hidden-file policy.
#include <QObject>
#include <QStringList>
#include <QThread>

namespace cf {

class WorkspaceIndexWorker;

class Workspace : public QObject {
    Q_OBJECT
public:
    explicit Workspace(QObject* parent = nullptr);
    ~Workspace() override;

    QString rootPath() const { return m_root; }
    bool isOpen() const { return !m_root.isEmpty(); }

    void openRoot(const QString& dir);      // "" closes the workspace
    QString relativePath(const QString& absPath) const;
    QString absolutePath(const QString& relPath) const;

    // File index snapshot (relative paths), used by Quick Open + Search.
    QStringList indexedFiles() const;
    bool isIndexReady() const;
    bool matchesExcludes(const QString& relPath) const;

    // Workspace settings scope.
    void applyWorkspaceSettings();

signals:
    void rootChanged(const QString& rootPath);
    void indexReady();
    void fileAdded(const QString& relativePath);
    void fileRemoved(const QString& relativePath);

private:
    void startIndexing();

    QString m_root;
    QStringList m_files;
    bool m_indexReady = false;
    QThread m_workerThread;
    WorkspaceIndexWorker* m_worker = nullptr;
};

}  // namespace cf
