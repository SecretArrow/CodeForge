#include "buildsys/BuildManager.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QStandardPaths>

#include "core/Logger.h"
#include "settings/SettingsManager.h"

namespace cf {

BuildManager::BuildManager(QObject* parent) : QObject(parent) {}

void BuildManager::setWorkspaceRoot(const QString& root)
{
    m_root = root;
    m_isCMake = !root.isEmpty() && QFileInfo::exists(root + QStringLiteral("/CMakeLists.txt"));
    m_buildDir = root.isEmpty() ? QString() : root + QStringLiteral("/build");
    stopRun();
    emit projectChanged(m_isCMake);
    CF_LOG_INFO(QStringLiteral("Build: workspace %1 (CMake=%2)").arg(root).arg(m_isCMake));
}

bool BuildManager::isBusy() const
{
    return (m_proc && m_proc->state() != QProcess::NotRunning) ||
           (m_runProc && m_runProc->state() != QProcess::NotRunning);
}

QString BuildManager::findTool(const QString& name)
{
    return QStandardPaths::findExecutable(name);
}

void BuildManager::start(const QStringList& args, const QString& workingDir)
{
    if (!m_isCMake || isBusy()) return;
    const QString cmake = findTool(QStringLiteral("cmake"));
    if (cmake.isEmpty()) {
        emit outputLine(QStringLiteral("[Build] cmake not found in PATH. Install CMake and reopen CodeForge.\n"));
        emit buildFinished(false, 1, 0);
        return;
    }

    m_errors = 0;
    m_warnings = 0;
    m_problems.clear();
    emit problemsUpdated(m_problems);

    m_proc = new QProcess(this);
    m_proc->setWorkingDirectory(workingDir.isEmpty() ? m_root : workingDir);
    m_proc->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_proc, &QProcess::readyRead, this, &BuildManager::onProcOutput);
    connect(m_proc, &QProcess::finished, this, &BuildManager::onProcFinished);
    CF_LOG_INFO(QStringLiteral("Build: cmake %1").arg(args.join(u' ')));
    m_proc->start(cmake, args);
}

void BuildManager::configure()
{
    if (!m_isCMake || isBusy()) return;
    QDir().mkpath(m_buildDir);
    emit stateChanged(QStringLiteral("Configuring"));

    QStringList args = { QStringLiteral("-S"), m_root, QStringLiteral("-B"), m_buildDir };
    const QString ninja = findTool(QStringLiteral("ninja"));
    if (!ninja.isEmpty()) args << QStringLiteral("-G") << QStringLiteral("Ninja");
    // else: use platform default generator.
    start(args, m_root);
}

void BuildManager::build(const QString& target)
{
    if (!m_isCMake || isBusy()) return;
    emit stateChanged(QStringLiteral("Building"));
    QStringList args = { QStringLiteral("--build"), m_buildDir, QStringLiteral("--config"), QStringLiteral("Release") };
    if (!target.isEmpty())
        args << QStringLiteral("--target") << target;
    start(args, m_root);
}

void BuildManager::rebuild()
{
    if (!m_isCMake || isBusy()) return;
    emit stateChanged(QStringLiteral("Building"));
    start({ QStringLiteral("--build"), m_buildDir, QStringLiteral("--clean-first"), QStringLiteral("--config"), QStringLiteral("Release") }, m_root);
}

void BuildManager::clean()
{
    if (!m_isCMake || isBusy()) return;
    emit stateChanged(QStringLiteral("Cleaning"));
    start({ QStringLiteral("--build"), m_buildDir, QStringLiteral("--target"), QStringLiteral("clean"), QStringLiteral("--config"), QStringLiteral("Release") }, m_root);
}

void BuildManager::cancel()
{
    if (m_proc) {
        m_proc->kill();
        emit outputLine(QStringLiteral("[Build] cancelled by user\n"));
    }
    stopRun();
}

void BuildManager::stopRun()
{
    if (m_runProc) {
        m_runProc->kill();
        m_runProc->deleteLater();
        m_runProc = nullptr;
        emit stateChanged(QStringLiteral("Idle"));
    }
}

void BuildManager::runExecutable(const QString& exePath, const QStringList& args)
{
    if (isBusy()) return;
    if (exePath.isEmpty() || !QFileInfo::exists(exePath)) {
        emit outputLine(QStringLiteral("[Run] executable not found: %1\n").arg(exePath));
        return;
    }
    emit stateChanged(QStringLiteral("Running"));
    m_runProc = new QProcess(this);
    m_runProc->setProcessChannelMode(QProcess::MergedChannels);
    m_runProc->setWorkingDirectory(m_root);
    connect(m_runProc, &QProcess::readyRead, this, [this]() {
        if (m_runProc)
            emit outputLine(QString::fromLocal8Bit(m_runProc->readAll()));
    });
    connect(m_runProc, &QProcess::finished, this, [this](int code, QProcess::ExitStatus st) {
        emit outputLine(QStringLiteral("\n[Run] finished with exit code %1%2\n")
                            .arg(code).arg(st == QProcess::CrashExit ? QStringLiteral(" (crashed)") : QString()));
        m_runProc->deleteLater();
        m_runProc = nullptr;
        emit stateChanged(QStringLiteral("Idle"));
        emit runFinished(code);
    });
    emit outputLine(QStringLiteral("[Run] %1 %2\n").arg(exePath, args.join(u' ')));
    m_runProc->start(exePath, args);
}

void BuildManager::onProcOutput()
{
    if (!m_proc) return;
    const QString text = QString::fromLocal8Bit(m_proc->readAll());
    emit outputLine(text);
    const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString& line : lines) parseProblems(line);
}

// MSVC:  file.cpp(12,34): error C2065: message   |  GCC/Ninja: file.cpp:12:34: error: message
void BuildManager::parseProblems(const QString& line)
{
    static const QRegularExpression msvc(QStringLiteral("^(.+?)\\((\\d+),(\\d+)\\):\\s+(error|warning)\\s+([A-Z]\\d+):\\s+(.+)$"));
    static const QRegularExpression gcc(QStringLiteral("^(.+?):(\\d+):(\\d+):\\s+(error|warning|fatal error):\\s+(.+)$"));

    QRegularExpressionMatch m = msvc.match(line);
    if (!m.hasMatch()) m = gcc.match(line);
    if (!m.hasMatch()) return;

    BuildProblem p;
    if (m.lastCapturedIndex() >= 6 && line.contains(QLatin1String("):"))) {
        p.file = QDir::fromNativeSeparators(m.captured(1));
        p.line = m.captured(2).toInt();
        p.column = m.captured(3).toInt();
        p.severity = m.captured(4) == QLatin1String("error") ? BuildProblem::Error : BuildProblem::Warning;
        p.code = m.captured(5);
        p.message = m.captured(6);
    } else {
        p.file = QDir::fromNativeSeparators(m.captured(1));
        p.line = m.captured(2).toInt();
        p.column = m.captured(3).toInt();
        p.severity = (m.captured(4) == QLatin1String("error") || m.captured(4) == QLatin1String("fatal error"))
                         ? BuildProblem::Error : BuildProblem::Warning;
        p.code = QString();
        p.message = m.captured(5);
    }
    bumpCounters(p);
}

void BuildManager::bumpCounters(const BuildProblem& p)
{
    if (p.severity == BuildProblem::Error) ++m_errors; else ++m_warnings;
    m_problems.append(p);
    emit problemsUpdated(m_problems);
}

void BuildManager::onProcFinished(int exitCode, QProcess::ExitStatus status)
{
    auto* proc = qobject_cast<QProcess*>(sender());
    if (proc) proc->deleteLater();
    m_proc = nullptr;

    const bool ok = (status == QProcess::NormalExit && exitCode == 0);
    emit outputLine(QStringLiteral("\n[Build] %1 — %2 errors, %3 warnings\n")
                        .arg(ok ? QStringLiteral("completed") : QStringLiteral("FAILED"))
                        .arg(m_errors)
                        .arg(m_warnings));
    emit stateChanged(QStringLiteral("Idle"));
    emit buildFinished(ok, m_errors, m_warnings);
}

QString BuildManager::detectExecutable() const
{
    // 1) explicit workspace setting wins.
    const QString configured = SettingsManager::instance().getString(QStringLiteral("run.executable"));
    if (!configured.isEmpty()) {
        const QString abs = QDir::isAbsolutePath(configured) ? configured : m_root + QLatin1Char('/') + configured;
        if (QFileInfo::exists(abs)) return abs;
    }
    // 2) heuristic: newest executable file in build dir (2 levels deep).
    qint64 newest = 0;
    QString found;
    QDirIterator it(m_buildDir, QDir::Files, QDirIterator::Subdirectories);
    int scanned = 0;
    while (it.hasNext() && scanned++ < 2000) {
        const QFileInfo info(it.next());
        if (!info.isExecutable()) continue;
        if (info.size() < 8192) continue;                       // skip wrappers
        if (info.lastModified().toMSecsSinceEpoch() > newest) {
            newest = info.lastModified().toMSecsSinceEpoch();
            found = info.absoluteFilePath();
        }
    }
    return found;
}

}  // namespace cf
