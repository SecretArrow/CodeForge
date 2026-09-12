#pragma once
// TaskRunner: workspace task automation (.codeforge/tasks.json), plus
// sensible default CMake tasks when the workspace root has a CMakeLists.txt.
// Tasks run asynchronously via QProcess; output is streamed to the output
// channel and each task is registered as a palette command.
#include <QProcess>
#include <QObject>
#include <QStringList>
#include <QVector>

namespace cf {

struct TaskDef {
    QString label;
    QString command;
    QStringList args;
    QString cwd;          // empty = workspace root
    bool useShell = false;
    bool isDefault = false;   // synthesized (not from tasks.json)
};

class TaskRunner : public QObject {
    Q_OBJECT
public:
    explicit TaskRunner(QObject* parent = nullptr);

    void setWorkspaceRoot(const QString& root);
    QVector<TaskDef> tasks() const { return m_tasks; }

    bool isRunning() const { return m_proc != nullptr; }
    void runTask(const QString& label);
    void cancel();

    static QVector<TaskDef> defaultsForRoot(const QString& root);   // pure, testable
    static bool writeTemplate(const QString& root);                 // creates tasks.json

signals:
    void taskOutput(const QString& label, const QString& line);
    void taskFinished(const QString& label, int exitCode);
    void tasksChanged();

private slots:
    void onReadyRead();
    void onFinished(int exitCode, QProcess::ExitStatus status);

private:
    void reload();
    static QVector<TaskDef> loadFile(const QString& path);

    QString m_root;
    QVector<TaskDef> m_tasks;
    QProcess* m_proc = nullptr;
    QString m_runningLabel;
};

}  // namespace cf
