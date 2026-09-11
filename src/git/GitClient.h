#pragma once
// Git integration via the git CLI (QProcess, fully asynchronous).
// Modular: the app works without git installed (hasGit() == false).
#include <QHash>
#include <QObject>
#include <QProcess>

namespace cf {

class GitClient : public QObject {
    Q_OBJECT
public:
    explicit GitClient(QObject* parent = nullptr);

    bool hasGit() const { return m_gitAvailable; }
    bool isRepository() const { return m_isRepo; }
    QString repoRoot() const { return m_repoRoot; }
    QString currentBranch() const { return m_branch; }
    bool isBusy() const { return m_busy; }

    // Detect repo for the given root (async).
    void refresh(const QString& workspaceRoot);

    // Asynchronous operations. Output goes to Output pane signals.
    void stage(const QStringList& relPaths);
    void unstage(const QStringList& relPaths);
    void discard(const QStringList& relPaths);           // git checkout --
    void commit(const QString& message);
    void pull();
    void push();
    void checkoutBranch(const QString& branch);
    QStringList branches();                              // synchronous; small output
    QString fileDiff(const QString& relPath, bool staged);  // synchronous text for diff viewer

signals:
    void stateChanged();                                  // repo/branch/status updated
    void statusUpdated(const QHash<QString, QString>& relPathState);
    void commandOutput(const QString& text);
    void commandFinished(bool ok, const QString& summary);

private slots:
    void onProcFinished(int exitCode, QProcess::ExitStatus status);
    void onProcError(QProcess::ProcessError error);
    void onReadyRead();

private:
    void run(const QStringList& args, const QStringList& paths, bool captureStatus);
    void startProcess(const QStringList& args);
    void finishStatus(const QByteArray& out);

    QProcess* m_proc = nullptr;
    bool m_gitAvailable = false;
    bool m_isRepo = false;
    bool m_busy = false;
    bool m_currentIsStatus = false;
    QString m_repoRoot;
    QString m_branch;
    QString m_workspaceRoot;
    QByteArray m_statusBuffer;
};

}  // namespace cf
