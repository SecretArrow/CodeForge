#pragma once
// DebuggerClient: GDB/LLDB driver over the MI2 protocol via QProcess
// (gdb --interpreter=mi2). Supports: start with program+args, breakpoints
// (wired to BreakpointStore), continue/pause/step/finish, stop, call stack
// and simple locals, expression evaluation, console output.
// Requires gdb (or lldb-mi compatible) on PATH or debugger.gdbPath setting.
#include <QProcess>
#include <QStringList>
#include <QVector>

#include "core/Breakpoints.h"

namespace cf {

struct DebugFrame {
    int level = 0;
    QString function;
    QString file;
    QString fullName;
    int line = 0;
    int address = 0;      // parsed from "*addr" when available; informational
};

struct DebugVariable {
    QString name;
    QString value;
    bool changed = false;   // highlighted between stops
};

class DebuggerClient : public QObject {
    Q_OBJECT
public:
    explicit DebuggerClient(QObject* parent = nullptr);
    ~DebuggerClient() override;

    // gdb executable resolution (setting debugger.gdbPath, then PATH; also
    // tries lldb-mi for LLDB setups).
    static QString findDebugger();

    bool isRunning() const { return m_proc != nullptr; }
    QString programPath() const { return m_program; }

    // Session control. start() returns false (and reports an error) when the
    // debugger binary cannot be found or the program does not exist.
    bool start(const QString& program, const QStringList& args, const QString& workDir);
    void stop();                       // terminate the debuggee + debugger
    void requestInterrupt();
    void requestContinue();
    void requestNext();                // step over
    void requestStep();                // step into
    void requestFinish();              // step out
    void evaluate(const QString& expression);
    void syncBreakpoints();            // push BreakpointStore state into gdb

signals:
    void started();
    void running();                          // debuggee (re)started
    void stopped(const QString& reason, const QString& file, int line);
    void exited(int exitCode);
    void stackReady(const QVector<cf::DebugFrame>& frames);
    void localsReady(const QVector<cf::DebugVariable>& vars);
    void evaluated(const QString& expression, const QString& value);
    void consoleOutput(const QString& line);   // human-readable console log
    void errorOutput(const QString& message);
    void sessionEnded();

private slots:
    void onReadyRead();
    void onErrorOccurred(QProcess::ProcessError e);
    void onProcFinished(int exitCode, QProcess::ExitStatus status);

private:
    void send(const QString& miCommand, int token);
    void handleLine(const QString& line);
    static QString unquote(const QString& value);
    static QHash<QString, QString> parseTuple(const QString& tuple);
    static void parseFrames(const QString& payload, QVector<DebugFrame>* frames);
    static void parseVariables(const QString& payload, QVector<DebugVariable>* vars);

    QProcess* m_proc = nullptr;
    QString m_program;
    int m_nextToken = 1;
    QStringList m_pendingAfterStop;   // stack/locals queries queued on stop
    bool m_inferiorRunning = false;
    QString m_consoleTail;
};

}  // namespace cf
