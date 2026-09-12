#include "tasks/TaskRunner.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "core/Logger.h"

namespace cf {

TaskRunner::TaskRunner(QObject* parent)
    : QObject(parent)
{
}

void TaskRunner::setWorkspaceRoot(const QString& root)
{
    if (m_root == root)
        return;
    m_root = root;
    reload();
}

void TaskRunner::reload()
{
    m_tasks.clear();
    if (!m_root.isEmpty()) {
        const QString file = m_root + QStringLiteral("/.codeforge/tasks.json");
        m_tasks = loadFile(file);
        if (m_tasks.isEmpty()) {
            // default tasks when CMake project detected
            m_tasks = defaultsForRoot(m_root);
        }
    }
    emit tasksChanged();
}

QVector<TaskDef> TaskRunner::loadFile(const QString& path)
{
    QVector<TaskDef> out;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return out;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    for (const auto& v : doc.object().value(QStringLiteral("tasks")).toArray()) {
        const QJsonObject o = v.toObject();
        TaskDef t;
        t.label = o.value(QStringLiteral("label")).toString();
        t.command = o.value(QStringLiteral("command")).toString();
        for (const auto& a : o.value(QStringLiteral("args")).toArray())
            t.args.append(a.toString());
        t.cwd = o.value(QStringLiteral("cwd")).toString();
        t.useShell = o.value(QStringLiteral("shell")).toBool(false);
        if (!t.label.isEmpty() && !t.command.isEmpty())
            out.append(t);
    }
    return out;
}

QVector<TaskDef> TaskRunner::defaultsForRoot(const QString& root)
{
    QVector<TaskDef> out;
    if (QFileInfo::exists(root + QStringLiteral("/CMakeLists.txt"))) {
        TaskDef configure;
        configure.label = QStringLiteral("CMake: Configure");
        configure.command = QStringLiteral("cmake");
        configure.args = { QStringLiteral("-S"), QStringLiteral("."),
                           QStringLiteral("-B"), QStringLiteral("build") };
        configure.isDefault = true;
        out.append(configure);

        TaskDef build;
        build.label = QStringLiteral("CMake: Build");
        build.command = QStringLiteral("cmake");
        build.args = { QStringLiteral("--build"), QStringLiteral("build") };
        build.isDefault = true;
        out.append(build);

        TaskDef clean;
        clean.label = QStringLiteral("CMake: Clean");
        clean.command = QStringLiteral("cmake");
        clean.args = { QStringLiteral("--build"), QStringLiteral("build"),
                       QStringLiteral("--target"), QStringLiteral("clean") };
        clean.isDefault = true;
        out.append(clean);
    }
    return out;
}

bool TaskRunner::writeTemplate(const QString& root)
{
    if (root.isEmpty())
        return false;
    QDir().mkpath(root + QStringLiteral("/.codeforge"));
    const QString path = root + QStringLiteral("/.codeforge/tasks.json");
    if (QFileInfo::exists(path))
        return true;
    QJsonArray tasks;
    QJsonObject build;
    build.insert(QStringLiteral("label"), QStringLiteral("Build"));
    build.insert(QStringLiteral("command"), QStringLiteral("cmake"));
    build.insert(QStringLiteral("args"), QJsonArray{ QStringLiteral("--build"),
                                                     QStringLiteral("build") });
    tasks.append(build);
    QJsonObject test;
    test.insert(QStringLiteral("label"), QStringLiteral("Run tests"));
    test.insert(QStringLiteral("command"), QStringLiteral("ctest"));
    test.insert(QStringLiteral("args"), QJsonArray{ QStringLiteral("--test-dir"),
                                                    QStringLiteral("build") });
    tasks.append(test);
    QJsonObject top;
    top.insert(QStringLiteral("version"), 1);
    top.insert(QStringLiteral("tasks"), tasks);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(QJsonDocument(top).toJson(QJsonDocument::Indented));
    return true;
}

void TaskRunner::runTask(const QString& label)
{
    if (m_proc) {
        emit taskOutput(label, QStringLiteral("[CodeForge] a task is already running"));
        return;
    }
    const TaskDef* found = nullptr;
    for (const TaskDef& t : m_tasks) {
        if (t.label == label) {
            found = &t;
            break;
        }
    }
    if (!found) {
        emit taskOutput(label, QStringLiteral("[CodeForge] unknown task"));
        return;
    }
    const TaskDef task = *found;
    m_runningLabel = label;
    QString cwd = task.cwd.isEmpty() ? m_root : task.cwd;
    if (QFileInfo(task.cwd).isRelative() && !m_root.isEmpty())
        cwd = m_root + QStringLiteral("/") + task.cwd;
    m_proc = new QProcess(this);
    connect(m_proc, &QProcess::readyRead, this, &TaskRunner::onReadyRead);
    connect(m_proc, &QProcess::finished, this, &TaskRunner::onFinished);
    m_proc->setWorkingDirectory(cwd);
    emit taskOutput(label, QStringLiteral("[CodeForge] running: %1 %2")
                               .arg(task.command, task.args.join(u' ')));
    if (task.useShell) {
#ifdef Q_OS_WIN
        const QString program = QStringLiteral("cmd.exe");
        QStringList args;
        args << QStringLiteral("/c") << task.command << task.args;
#else
        const QString program = QStringLiteral("sh");
        QStringList args;
        args << QStringLiteral("-c") << (task.command + QStringLiteral(" ")
                                         + task.args.join(u' '));
#endif
        m_proc->start(program, args);
    } else {
        m_proc->start(task.command, task.args);
    }
    if (!m_proc->waitForStarted(3000)) {
        emit taskOutput(label, QStringLiteral("[CodeForge] failed to start: %1")
                                   .arg(task.command));
        m_proc->deleteLater();
        m_proc = nullptr;
        emit taskFinished(label, -1);
    }
}

void TaskRunner::cancel()
{
    if (m_proc)
        m_proc->kill();
}

void TaskRunner::onReadyRead()
{
    if (!m_proc)
        return;
    while (m_proc->canReadLine()) {
        const QString line = QString::fromUtf8(m_proc->readLine()).trimmed();
        if (!line.isEmpty())
            emit taskOutput(m_runningLabel, line);
    }
}

void TaskRunner::onFinished(int exitCode, QProcess::ExitStatus status)
{
    if (!m_proc)
        return;
    m_proc->deleteLater();
    m_proc = nullptr;
    const QString label = m_runningLabel;
    m_runningLabel.clear();
    if (status == QProcess::CrashExit)
        emit taskOutput(label, QStringLiteral("[CodeForge] task was killed"));
    emit taskFinished(label, exitCode);
    Logger::instance().info(QStringLiteral("task '%1' exit %2").arg(label).arg(exitCode));
}

}  // namespace cf
