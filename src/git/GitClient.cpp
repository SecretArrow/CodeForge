#include "git/GitClient.h"

#include <QDir>
#include <QRegularExpression>

#include "core/Logger.h"

namespace cf {

GitClient::GitClient(QObject* parent) : QObject(parent)
{
    // Detect git once.
    QProcess check;
    check.start(QStringLiteral("git"), { QStringLiteral("--version") });
    if (check.waitForFinished(2000) && check.exitStatus() == QProcess::NormalExit) {
        m_gitAvailable = check.exitCode() == 0;
    }
    if (m_gitAvailable)
        CF_LOG_INFO(QStringLiteral("Git: git CLI detected"));
    else
        CF_LOG_INFO(QStringLiteral("Git: git CLI not found, integration disabled"));
}

void GitClient::refresh(const QString& workspaceRoot)
{
    m_workspaceRoot = workspaceRoot;
    m_isRepo = false;
    m_repoRoot.clear();
    m_branch.clear();
    if (!m_gitAvailable || workspaceRoot.isEmpty()) {
        emit stateChanged();
        return;
    }

    QProcess resolve;
    resolve.setWorkingDirectory(workspaceRoot);
    resolve.start(QStringLiteral("git"), { QStringLiteral("rev-parse"), QStringLiteral("--show-toplevel") });
    if (!resolve.waitForFinished(3000)) { emit stateChanged(); return; }
    if (resolve.exitCode() != 0) { emit stateChanged(); return; }

    m_repoRoot = QString::fromLocal8Bit(resolve.readAllStandardOutput().trimmed());
    m_isRepo = true;

    QProcess branch;
    branch.setWorkingDirectory(m_repoRoot);
    branch.start(QStringLiteral("git"), { QStringLiteral("rev-parse"), QStringLiteral("--abbrev-ref"), QStringLiteral("HEAD") });
    if (branch.waitForFinished(3000) && branch.exitCode() == 0)
        m_branch = QString::fromLocal8Bit(branch.readAllStandardOutput().trimmed());

    // Status (porcelain v1 -z, includes untracked).
    QProcess status;
    status.setWorkingDirectory(m_repoRoot);
    status.start(QStringLiteral("git"), { QStringLiteral("status"), QStringLiteral("--porcelain=v1"), QStringLiteral("-z"), QStringLiteral("--untracked-files=all") });
    QHash<QString, QString> relState;
    if (status.waitForFinished(5000) && status.exitCode() == 0) {
        const QByteArray out = status.readAllStandardOutput();
        const QList<QByteArray> parts = out.split('\0');
        for (int i = 0; i + 1 < parts.size(); i += 2) {
            const QByteArray xy = parts.at(i);
            const QByteArray path = parts.at(i + 1);
            if (path.isEmpty()) continue;
            QString file = QString::fromLocal8Bit(path);
            if (file.startsWith(QLatin1Char('"'))) file = file.mid(1, file.size() - 2);
            if (m_workspaceRoot == m_repoRoot)
                relState.insert(file, QString::fromLatin1(xy.mid(0, 1)));
            else
                relState.insert(QDir(m_repoRoot).relativeFilePath(m_repoRoot + QLatin1Char('/') + file), QString::fromLatin1(xy.mid(0, 1)));
        }
    }
    emit statusUpdated(relState);
    emit stateChanged();
}

void GitClient::run(const QStringList& args, const QStringList& paths, bool captureStatus)
{
    if (!m_isRepo || m_busy) {
        emit commandFinished(false, QStringLiteral("git busy or not available"));
        return;
    }
    m_busy = true;
    m_currentIsStatus = captureStatus;
    m_statusBuffer.clear();

    QStringList fullArgs = args;
    if (!paths.isEmpty()) {
        fullArgs << QStringLiteral("--");
        for (const QString& p : paths) fullArgs << p;
    }

    m_proc = new QProcess(this);
    m_proc->setWorkingDirectory(m_repoRoot);
    connect(m_proc, &QProcess::finished, this, &GitClient::onProcFinished);
    connect(m_proc, &QProcess::readyReadStandardOutput, this, &GitClient::onReadyRead);
    connect(m_proc, &QProcess::readyReadStandardError, this, &GitClient::onReadyRead);
    connect(m_proc, &QProcess::errorOccurred, this, &GitClient::onProcError);
    CF_LOG_INFO(QStringLiteral("Git: %1").arg(fullArgs.join(u' ')));
    m_proc->start(QStringLiteral("git"), fullArgs);
}

void GitClient::startProcess(const QStringList& args)
{
    m_busy = true;
    m_currentIsStatus = false;
    m_proc = new QProcess(this);
    m_proc->setWorkingDirectory(m_repoRoot);
    connect(m_proc, &QProcess::finished, this, &GitClient::onProcFinished);
    connect(m_proc, &QProcess::readyReadStandardOutput, this, &GitClient::onReadyRead);
    connect(m_proc, &QProcess::readyReadStandardError, this, &GitClient::onReadyRead);
    connect(m_proc, &QProcess::errorOccurred, this, &GitClient::onProcError);
    m_proc->start(QStringLiteral("git"), args);
}

void GitClient::onReadyRead()
{
    auto* proc = qobject_cast<QProcess*>(sender());
    if (!proc) return;
    const QByteArray out = proc->readAllStandardOutput() + proc->readAllStandardError();
    if (m_currentIsStatus) m_statusBuffer += out;
    else emit commandOutput(QString::fromLocal8Bit(out));
}

void GitClient::onProcError(QProcess::ProcessError error)
{
    Q_UNUSED(error);
    emit commandOutput(QStringLiteral("git: process error\n"));
}

void GitClient::onProcFinished(int exitCode, QProcess::ExitStatus status)
{
    auto* proc = qobject_cast<QProcess*>(sender());
    m_busy = false;
    const bool ok = (status == QProcess::NormalExit && exitCode == 0);
    if (proc) {
        proc->deleteLater();
        m_proc = nullptr;
    }
    emit commandFinished(ok, ok ? QStringLiteral("OK") : QStringLiteral("exit %1").arg(exitCode));
    if (ok && m_currentIsStatus) {
        refresh(m_workspaceRoot);
    } else if (ok) {
        // Any successful mutation changes status; refresh.
        refresh(m_workspaceRoot);
    }
}

void GitClient::stage(const QStringList& relPaths) { run({ QStringLiteral("add") }, relPaths, true); }
void GitClient::unstage(const QStringList& relPaths) { run({ QStringLiteral("reset"), QStringLiteral("-q"), QStringLiteral("HEAD") }, relPaths, true); }
void GitClient::discard(const QStringList& relPaths) { run({ QStringLiteral("checkout"), QStringLiteral("--") }, relPaths, true); }

void GitClient::commit(const QString& message)
{
    if (!m_isRepo || m_busy) return;
    startProcess({ QStringLiteral("commit"), QStringLiteral("-m"), message });
}

void GitClient::pull()
{
    if (!m_isRepo || m_busy) return;
    startProcess({ QStringLiteral("pull") });
}

void GitClient::push()
{
    if (!m_isRepo || m_busy) return;
    startProcess({ QStringLiteral("push") });
}

void GitClient::checkoutBranch(const QString& branch)
{
    if (!m_isRepo || m_busy) return;
    startProcess({ QStringLiteral("checkout"), branch });
}

QStringList GitClient::branches()
{
    QStringList out;
    if (!m_isRepo) return out;
    QProcess p;
    p.setWorkingDirectory(m_repoRoot);
    p.start(QStringLiteral("git"), { QStringLiteral("branch"), QStringLiteral("--format=%(refname:short)") });
    if (p.waitForFinished(3000) && p.exitCode() == 0) {
        const QString text = QString::fromLocal8Bit(p.readAllStandardOutput());
        for (const QString& line : text.split(QLatin1Char('\n'), Qt::SkipEmptyParts))
            out.append(line.trimmed());
    }
    return out;
}

QString GitClient::fileDiff(const QString& relPath, bool staged)
{
    if (!m_isRepo) return QString();
    QProcess p;
    p.setWorkingDirectory(m_repoRoot);
    QStringList args = { QStringLiteral("diff"), QStringLiteral("--no-color") };
    if (staged) args << QStringLiteral("--cached");
    args << QStringLiteral("--") << relPath;
    p.start(QStringLiteral("git"), args);
    if (p.waitForFinished(5000) && p.exitCode() == 0)
        return QString::fromLocal8Bit(p.readAllStandardOutput());
    return QString();
}

}  // namespace cf
