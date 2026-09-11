#pragma once
// CMake project support: detection, configure/build/clean/run — all
// asynchronous via QProcess. Parses compiler output into problems.
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVector>

namespace cf {

struct BuildProblem {
    enum Severity { Warning, Error } severity = Warning;
    QString file;
    int line = 0;
    int column = 0;
    QString code;
    QString message;
};

class BuildManager : public QObject {
    Q_OBJECT
public:
    explicit BuildManager(QObject* parent = nullptr);

    void setWorkspaceRoot(const QString& root);
    bool isCMakeProject() const { return m_isCMake; }
    QString buildDir() const { return m_buildDir; }
    void setBuildDir(const QString& dir) { m_buildDir = dir; }

    bool isBusy() const;
    void configure();
    void build(const QString& target = QString());
    void rebuild();
    void clean();
    void cancel();
    void runExecutable(const QString& exePath, const QStringList& args = {});
    void stopRun();

    // Run configuration: preferred executable (workspace setting wins).
    QString detectExecutable() const;   // heuristic: newest executable in build dir

signals:
    void projectChanged(bool isCMake);
    void outputLine(const QString& line);             // configure/build/run output
    void stateChanged(const QString& state);          // "Idle", "Configuring", "Building", "Running"
    void problemsUpdated(const QVector<cf::BuildProblem>& problems);
    void buildFinished(bool ok, int errors, int warnings);
    void runFinished(int exitCode);

private slots:
    void onProcOutput();
    void onProcFinished(int exitCode, QProcess::ExitStatus status);

private:
    void start(const QStringList& args, const QString& workingDir);
    void parseProblems(const QString& line);
    void bumpCounters(const BuildProblem& p);
    static QString findTool(const QString& name);

    QProcess* m_proc = nullptr;
    QProcess* m_runProc = nullptr;
    QString m_root;
    QString m_buildDir;
    bool m_isCMake = false;
    QString m_state;
    int m_errors = 0;
    int m_warnings = 0;
    QVector<BuildProblem> m_problems;
};

}  // namespace cf
