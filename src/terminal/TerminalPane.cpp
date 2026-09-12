#include "terminal/TerminalPane.h"

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QMenu>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>

#include "core/Logger.h"
#include "settings/SettingsManager.h"

namespace cf {

TerminalWidget::TerminalWidget(const QString& workingDir, QWidget* parent)
    : QWidget(parent), m_workingDir(workingDir)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_output = new QPlainTextEdit(this);
    m_output->setReadOnly(true);
    m_output->setFrameStyle(QFrame::NoFrame);
    QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    mono.setPointSize(SettingsManager::instance().getInt(QStringLiteral("terminal.fontSize")));
    m_output->setFont(mono);
    layout->addWidget(m_output, 1);

    m_input = new QLineEdit(this);
    m_input->setFont(mono);
    m_input->setFrame(false);
    m_input->setPlaceholderText(tr("Type a command and press Enter"));
    layout->addWidget(m_input);

    connect(m_input, &QLineEdit::returnPressed, this, &TerminalWidget::sendCommand);
    m_output->installEventFilter(this);
}

bool TerminalWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_output && event->type() == QEvent::ContextMenu) {
        QMenu menu(this);
        menu.addAction(tr("Copy"), this, [this]() {
            QApplication::clipboard()->setText(m_output->textCursor().selectedText());
        });
        menu.addAction(tr("Clear"), this, &TerminalWidget::clear);
        menu.exec(static_cast<QContextMenuEvent*>(event)->globalPos());
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void TerminalWidget::startShell(const QString& shellId)
{
    kill();

    QString program;
    QStringList args;
    m_shellId = shellId;

#ifdef Q_OS_WIN
    if (shellId == QLatin1String("cmd")) {
        program = qEnvironmentVariable("ComSpec");
        if (program.isEmpty()) program = QStringLiteral("cmd.exe");
    } else if (shellId == QLatin1String("gitbash")) {
        program = QStringLiteral("bash.exe");
    } else if (shellId == QLatin1String("pwsh")) {
        program = QStringLiteral("pwsh.exe");
        args << QStringLiteral("-NoLogo");
    } else {   // auto / powershell
        program = QStringLiteral("powershell.exe");
        args << QStringLiteral("-NoLogo");
    }
#else
    if (shellId == QLatin1String("bash")) program = QStringLiteral("bash");
    else program = qEnvironmentVariable("SHELL");
    if (program.isEmpty()) program = QStringLiteral("/bin/sh");
#endif

    m_proc = new QProcess(this);
    m_proc->setProgram(program);
    m_proc->setArguments(args);
    m_proc->setWorkingDirectory(m_workingDir.isEmpty() ? QDir::homePath() : m_workingDir);
    m_proc->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_proc, &QProcess::readyRead, this, &TerminalWidget::onReadyRead);
    connect(m_proc, &QProcess::finished, this, &TerminalWidget::onFinished);
    m_proc->start();
    CF_LOG_INFO(QStringLiteral("Terminal: started %1 in %2").arg(program, m_workingDir));
}

void TerminalWidget::onReadyRead()
{
    if (!m_proc) return;
    const QByteArray raw = m_proc->readAll();
    appendOutput(QString::fromLocal8Bit(raw));
}

void TerminalWidget::appendOutput(const QString& text)
{
    m_output->moveCursor(QTextCursor::End);
    m_output->insertPlainText(text);
    m_output->moveCursor(QTextCursor::End);
}

void TerminalWidget::appendPromptEcho()
{
    const QString dir = m_workingDir.isEmpty() ? QDir::homePath() : m_workingDir;
    appendOutput(QStringLiteral("\n%1> %2\n").arg(dir, m_input->text()));
}

void TerminalWidget::sendCommand()
{
    const QString cmd = m_input->text();
    if (cmd.isEmpty()) return;
    m_history.append(cmd);
    m_historyIndex = -1;
    appendPromptEcho();

    if (m_proc && m_proc->state() == QProcess::Running) {
        m_proc->write(cmd.toLocal8Bit() + QByteArrayLiteral("\n"));
    } else {
        appendOutput(tr("[process not running]\n"));
    }
    m_input->clear();
}

void TerminalWidget::onFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(exitCode);
    appendOutput(status == QProcess::CrashExit ? tr("\n[shell terminated unexpectedly]\n")
                                               : tr("\n[shell exited]\n"));
}

void TerminalWidget::kill()
{
    if (m_proc) {
        m_proc->kill();
        m_proc->waitForFinished(1000);
        m_proc->deleteLater();
        m_proc = nullptr;
    }
}

void TerminalWidget::restart()
{
    startShell(m_shellId.isEmpty() ? QStringLiteral("auto") : m_shellId);
}

void TerminalWidget::clear()
{
    m_output->clear();
}

// ---------------- TerminalPane ----------------

TerminalPane::TerminalPane(QWidget* parent) : QTabWidget(parent)
{
    setTabsClosable(true);
    setMovable(false);
    setDocumentMode(true);

    connect(this, &QTabWidget::tabCloseRequested, this, [this](int idx) {
        QWidget* w = widget(idx);
        removeTab(idx);
        delete w;
    });
}

void TerminalPane::setWorkingDir(const QString& dir)
{
    m_workingDir = dir;
}

QString TerminalPane::resolveShell() const
{
    const QString pref = SettingsManager::instance().getString(QStringLiteral("terminal.shell"));
    if (pref != QLatin1String("auto")) return pref;
#ifdef Q_OS_WIN
    return QStringLiteral("powershell");
#else
    return QStringLiteral("bash");
#endif
}

void TerminalPane::newTerminal()
{
    ++m_counter;
    auto* term = new TerminalWidget(m_workingDir, this);
    term->startShell(resolveShell());
    const int idx = addTab(term, QStringLiteral("Terminal %1").arg(m_counter));
    setCurrentIndex(idx);
}

void TerminalPane::killCurrent()
{
    auto* term = qobject_cast<TerminalWidget*>(currentWidget());
    if (term) term->kill();
}

void TerminalPane::restartCurrent()
{
    auto* term = qobject_cast<TerminalWidget*>(currentWidget());
    if (term) term->restart();
}

void TerminalPane::clearCurrent()
{
    auto* term = qobject_cast<TerminalWidget*>(currentWidget());
    if (term) term->clear();
}

}  // namespace cf
