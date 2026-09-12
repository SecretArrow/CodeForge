#include "debug/DebuggerClient.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>

#include "core/Logger.h"
#include "settings/SettingsManager.h"

namespace cf {

DebuggerClient::DebuggerClient(QObject* parent)
    : QObject(parent)
{
}

DebuggerClient::~DebuggerClient()
{
    stop();
}

QString DebuggerClient::findDebugger()
{
    const QString configured =
        SettingsManager::instance().getString(QStringLiteral("debugger.gdbPath"));
    if (!configured.isEmpty() && QFileInfo::exists(configured))
        return configured;
    for (const QString& name : { QStringLiteral("gdb"), QStringLiteral("lldb-mi"),
                                 QStringLiteral("lldb-mi.exe") }) {
        const QString p = QStandardPaths::findExecutable(name);
        if (!p.isEmpty())
            return p;
    }
    return QString();
}

bool DebuggerClient::start(const QString& program, const QStringList& args,
                           const QString& workDir)
{
    if (m_proc) {
        emit errorOutput(tr("A debug session is already running."));
        return false;
    }
    const QString dbg = findDebugger();
    if (dbg.isEmpty()) {
        emit errorOutput(tr("No debugger found. Install gdb (e.g. via MSYS2, MinGW-w64, WSL or "
                            "your toolchain) or set debugger.gdbPath in settings."));
        return false;
    }
    if (!QFileInfo::exists(program)) {
        emit errorOutput(tr("Program not found: %1").arg(program));
        return false;
    }
    m_program = program;
    m_nextToken = 1;
    m_inferiorRunning = false;

    m_proc = new QProcess(this);
    m_proc->setWorkingDirectory(workDir);
    connect(m_proc, &QProcess::readyRead, this, &DebuggerClient::onReadyRead);
    connect(m_proc, &QProcess::errorOccurred, this, &DebuggerClient::onErrorOccurred);
    connect(m_proc, &QProcess::finished, this, &DebuggerClient::onProcFinished);

    QStringList miArgs;
    miArgs << QStringLiteral("--quiet") << QStringLiteral("--nx")
           << QStringLiteral("--interpreter=mi2");
    if (!args.isEmpty())
        miArgs << QStringLiteral("--args") << program << args;
    else
        miArgs << program;
    m_proc->start(dbg, miArgs);
    if (!m_proc->waitForStarted(5000)) {
        emit errorOutput(tr("Failed to start %1").arg(dbg));
        m_proc->deleteLater();
        m_proc = nullptr;
        return false;
    }
    // Load symbols for the --args-less variant as well; harmless when set.
    send(QStringLiteral("-file-exec-and-symbols \"%1\"").arg(program), m_nextToken++);
    syncBreakpoints();
    emit started();
    return true;
}

void DebuggerClient::stop()
{
    if (!m_proc)
        return;
    send(QStringLiteral("-gdb-exit"), m_nextToken++);
    if (!m_proc->waitForFinished(2000))
        m_proc->kill();
}

void DebuggerClient::send(const QString& miCommand, int token)
{
    if (!m_proc)
        return;
    const QString line = QStringLiteral("%1%2\n").arg(token).arg(miCommand);
    m_proc->write(line.toUtf8());
}

void DebuggerClient::syncBreakpoints()
{
    if (!m_proc)
        return;
    // Create new ones; gdb ids are tracked in the store via debuggerId.
    for (const Breakpoint& bp : BreakpointStore::instance().all()) {
        if (bp.debuggerId >= 0)
            continue;
        const int token = m_nextToken++;
        send(QStringLiteral("-break-insert \"%1:%2\"").arg(bp.filePath).arg(bp.line + 1), token);
        // The reply (token-matched) assigns the id; parse it in handleLine.
    }
}

void DebuggerClient::requestInterrupt()
{
    if (m_proc && m_inferiorRunning)
        send(QStringLiteral("-exec-interrupt"), m_nextToken++);
}

void DebuggerClient::requestContinue()
{
    if (!m_proc)
        return;
    m_inferiorRunning = true;
    send(QStringLiteral("-exec-continue"), m_nextToken++);
}

void DebuggerClient::requestNext()
{
    if (m_proc)
        send(QStringLiteral("-exec-next"), m_nextToken++);
}

void DebuggerClient::requestStep()
{
    if (m_proc)
        send(QStringLiteral("-exec-step"), m_nextToken++);
}

void DebuggerClient::requestFinish()
{
    if (m_proc)
        send(QStringLiteral("-exec-finish"), m_nextToken++);
}

void DebuggerClient::evaluate(const QString& expression)
{
    if (m_proc)
        send(QStringLiteral("-data-evaluate-expression \"%1\"").arg(expression), m_nextToken++);
}

void DebuggerClient::onErrorOccurred(QProcess::ProcessError e)
{
    if (e == QProcess::FailedToStart)
        emit errorOutput(tr("Debugger process failed to start."));
}

void DebuggerClient::onProcFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(status);
    m_proc->deleteLater();
    m_proc = nullptr;
    m_inferiorRunning = false;
    emit sessionEnded();
    Logger::instance().info(QStringLiteral("debugger exit %1").arg(exitCode));
}

void DebuggerClient::onReadyRead()
{
    if (!m_proc)
        return;
    while (m_proc->canReadLine()) {
        const QString line = QString::fromUtf8(m_proc->readLine()).trimmed();
        if (!line.isEmpty())
            handleLine(line);
    }
}

static QString stripOuterQuotes(const QString& s)
{
    if (s.size() >= 2 && s.startsWith(QLatin1Char('"')) && s.endsWith(QLatin1Char('"')))
        return s.mid(1, s.size() - 2);
    return s;
}

QString DebuggerClient::unquote(const QString& value)
{
    QString out = stripOuterQuotes(value);
    out.replace(QLatin1String("\\n"), QLatin1String("\n"));
    out.replace(QLatin1String("\\t"), QLatin1String("\t"));
    out.replace(QLatin1String("\\\""), QLatin1String("\""));
    out.replace(QLatin1String("\\\\"), QLatin1String("\\"));
    return out;
}

QHash<QString, QString> DebuggerClient::parseTuple(const QString& tuple)
{
    // Parse a flat MI tuple of key="value" pairs (enough for frame=/locals=).
    QHash<QString, QString> out;
    static const QRegularExpression re(
        QStringLiteral("([A-Za-z0-9_-]+)=\"((?:[^\"\\\\]|\\\\.)*)\""));
    auto it = re.globalMatch(tuple);
    while (it.hasNext()) {
        const auto m = it.next();
        out.insert(m.captured(1), m.captured(2));
    }
    return out;
}

void DebuggerClient::parseFrames(const QString& payload, QVector<DebugFrame>* frames)
{
    static const QRegularExpression frameRe(
        QStringLiteral("frame=\\{([^}]*)\\}"));
    auto it = frameRe.globalMatch(payload);
    while (it.hasNext()) {
        const auto m = it.next();
        const auto fields = parseTuple(m.captured(1));
        DebugFrame f;
        f.level = fields.value(QStringLiteral("level")).toInt();
        f.function = unquote(fields.value(QStringLiteral("func")));
        f.file = unquote(fields.value(QStringLiteral("file")));
        f.fullName = unquote(fields.value(QStringLiteral("fullname")));
        f.line = fields.value(QStringLiteral("line")).toInt();
        bool okAddr = false;
        const QString addr = fields.value(QStringLiteral("addr"));
        if (addr.startsWith(QLatin1String("0x")))
            f.address = addr.toULongLong(&okAddr, 16);
        frames->append(f);
    }
}

void DebuggerClient::parseVariables(const QString& payload, QVector<DebugVariable>* vars)
{
    static const QRegularExpression varRe(
        QStringLiteral("\\{name=\"((?:[^\"\\\\]|\\\\.)*)\",value=\"((?:[^\"\\\\]|\\\\.)*)\""));
    auto it = varRe.globalMatch(payload);
    while (it.hasNext()) {
        const auto m = it.next();
        DebugVariable v;
        v.name = unquote(m.captured(1));
        v.value = unquote(m.captured(2));
        vars->append(v);
    }
}

void DebuggerClient::handleLine(const QString& line)
{
    // Stream records: ~"console text"
    if (line.startsWith(QLatin1Char('~'))) {
        const QString text = unquote(line.mid(1));
        emit consoleOutput(text);
        return;
    }
    // Log records
    if (line.startsWith(QLatin1Char('&'))) {
        emit consoleOutput(QStringLiteral("(gdb) %1").arg(unquote(line.mid(1))));
        return;
    }
    // Result records: ^done, ^error, ^running
    if (line.startsWith(QLatin1String("^error"))) {
        const QString msg = unquote(line.mid(line.indexOf(QLatin1Char('=')) + 1));
        emit errorOutput(msg);
        return;
    }
    if (line.startsWith(QLatin1String("^done,stack=")) || line.startsWith(QLatin1String("^done,stack=["))
        || (line.startsWith(QLatin1String("^done,")) && line.contains(QLatin1String("frame={")))) {
        QVector<DebugFrame> frames;
        parseFrames(line, &frames);
        if (!frames.isEmpty())
            emit stackReady(frames);
        return;
    }
    if (line.startsWith(QLatin1String("^done,variables="))
        || (line.startsWith(QLatin1String("^done,")) && line.contains(QLatin1String("name=\"")))) {
        QVector<DebugVariable> vars;
        parseVariables(line, &vars);
        if (!vars.isEmpty())
            emit localsReady(vars);
        return;
    }
    if (line.startsWith(QLatin1String("^done,value="))) {
        // -data-evaluate-expression reply
        const int eq = line.indexOf(QLatin1Char('='));
        emit evaluated(QString(), unquote(line.mid(eq + 1)));
        return;
    }
    if (line.startsWith(QLatin1String("^done"))) {
        return;
    }
    if (line.startsWith(QLatin1String("^running"))) {
        m_inferiorRunning = true;
        emit running();
        return;
    }
    // Async records: *stopped,*running
    if (line.startsWith(QLatin1String("*running"))) {
        m_inferiorRunning = true;
        emit running();
        return;
    }
    if (line.startsWith(QLatin1String("*stopped"))) {
        m_inferiorRunning = false;
        // reason, frame
        static const QRegularExpression reasonRe(QStringLiteral("reason=\"([a-z-]+)\""));
        const auto rm = reasonRe.match(line);
        QString reason = rm.hasMatch() ? rm.captured(1) : QStringLiteral("stop");
        static const QRegularExpression frameRe(QStringLiteral("frame=\\{([^}]*)\\}"));
        const auto fm = frameRe.match(line);
        QString file, full;
        int ln = 0;
        if (fm.hasMatch()) {
            const auto fields = parseTuple(fm.captured(1));
            file = unquote(fields.value(QStringLiteral("file")));
            full = unquote(fields.value(QStringLiteral("fullname")));
            ln = fields.value(QStringLiteral("line")).toInt() - 1;
        }
        if (reason == QLatin1String("exited-normally")
            || reason == QLatin1String("exited")) {
            emit exited(0);
        }
        emit stopped(reason, full.isEmpty() ? file : full, ln);
        // follow up: stack + locals
        send(QStringLiteral("-stack-list-frames"), m_nextToken++);
        send(QStringLiteral("-stack-list-variables --simple-values"), m_nextToken++);
        return;
    }
    // Breakpoint creation: token + ^done,bkpt={number="2",...}
    static const QRegularExpression bkptRe(
        QStringLiteral("^([0-9]+)\\^done,bkpt=\\{number=\"([0-9]+)\",.*?file=\"((?:[^\"\\\\]|\\\\.)*)\",.*?line=\"([0-9]+)\""));
    const auto bm = bkptRe.match(line);
    if (bm.hasMatch()) {
        const QString file = unquote(bm.captured(3));
        const int gdbLine = bm.captured(4).toInt() - 1;
        const int gdbId = bm.captured(2).toInt();
        // match the store entry by (file, line)
        for (Breakpoint& bp : BreakpointStore::instance().all()) {
            if (bp.debuggerId < 0 && bp.filePath == file
                && (bp.line == gdbLine || bp.line + 1 == gdbLine + 1)) {
                BreakpointStore::instance().setDebuggerId(bp.filePath, bp.line, gdbId);
                break;
            }
        }
        return;
    }
    // =breakpoint-deleted,id="1"
    static const QRegularExpression delRe(QStringLiteral("id=\"([0-9]+)\""));
    if (line.startsWith(QLatin1String("=breakpoint-deleted"))) {
        const auto dm = delRe.match(line);
        if (dm.hasMatch()) {
            const int gdbId = dm.captured(1).toInt();
            for (const Breakpoint& bp : BreakpointStore::instance().all()) {
                if (bp.debuggerId == gdbId) {
                    BreakpointStore::instance().remove(bp.filePath, bp.line);
                    break;
                }
            }
        }
        return;
    }
}

}  // namespace cf
