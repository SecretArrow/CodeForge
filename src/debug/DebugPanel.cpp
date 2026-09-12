#include "debug/DebugPanel.h"

#include <QComboBox>
#include <QFontDatabase>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QStackedWidget>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "core/Breakpoints.h"
#include "core/Logger.h"
#include "debug/DebuggerClient.h"
#include "ui/Icons.h"

namespace cf {

DebugPanel::DebugPanel(QWidget* parent)
    : QWidget(parent)
    , m_client(new DebuggerClient(this))
{
    buildUi();
    connect(m_client, &DebuggerClient::started, this, &DebugPanel::onStarted);
    connect(m_client, &DebuggerClient::running, this, &DebugPanel::onRunning);
    connect(m_client, &DebuggerClient::stopped, this, &DebugPanel::onStopped);
    connect(m_client, &DebuggerClient::exited, this, &DebugPanel::onExited);
    connect(m_client, &DebuggerClient::stackReady, this, &DebugPanel::onStack);
    connect(m_client, &DebuggerClient::localsReady, this, &DebugPanel::onLocals);
    connect(m_client, &DebuggerClient::evaluated, this, &DebugPanel::onEvaluated);
    connect(m_client, &DebuggerClient::consoleOutput, this, &DebugPanel::onConsole);
    connect(m_client, &DebuggerClient::errorOutput, this, &DebugPanel::onError);
    connect(m_client, &DebuggerClient::sessionEnded, this, &DebugPanel::onSessionEnded);
    connect(&BreakpointStore::instance(), &BreakpointStore::changed, this,
            &DebugPanel::onBreakpointsChanged);
}

void DebugPanel::buildUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto* form = new QFormLayout;
    form->setContentsMargins(0, 0, 0, 0);
    m_program = new QComboBox(this);
    m_program->setEditable(true);
    m_program->setInsertPolicy(QComboBox::NoInsert);
    m_args = new QLineEdit(this);
    m_args->setPlaceholderText(tr("program arguments (optional)"));
    form->addRow(tr("Program"), m_program);
    form->addRow(tr("Arguments"), m_args);
    layout->addLayout(form);

    auto* toolbar = new QHBoxLayout;
    m_startBtn = new QToolButton(this);
    m_startBtn->setText(tr("Start"));
    m_startBtn->setIcon(Icons::icon(Icons::Name::Play));
    m_startBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_continueBtn = new QToolButton(this);
    m_continueBtn->setText(tr("Continue"));
    m_pauseBtn = new QToolButton(this);
    m_pauseBtn->setText(tr("Pause"));
    m_stepOverBtn = new QToolButton(this);
    m_stepOverBtn->setText(tr("Step over"));
    m_stepIntoBtn = new QToolButton(this);
    m_stepIntoBtn->setText(tr("Step in"));
    m_stepOutBtn = new QToolButton(this);
    m_stepOutBtn->setText(tr("Step out"));
    m_stopBtn = new QToolButton(this);
    m_stopBtn->setText(tr("Stop"));
    for (QToolButton* b : { m_continueBtn, m_pauseBtn, m_stepOverBtn, m_stepIntoBtn,
                            m_stepOutBtn, m_stopBtn })
        b->setEnabled(false);
    toolbar->addWidget(m_startBtn);
    toolbar->addWidget(m_continueBtn);
    toolbar->addWidget(m_pauseBtn);
    toolbar->addWidget(m_stepOverBtn);
    toolbar->addWidget(m_stepIntoBtn);
    toolbar->addWidget(m_stepOutBtn);
    toolbar->addWidget(m_stopBtn);
    layout->addLayout(toolbar);

    m_state = new QLabel(tr("Debugger: gdb / lldb-mi (MI2). Click a margin in the editor to "
                            "toggle breakpoints."), this);
    layout->addWidget(m_state);

    m_stack = new QStackedWidget(this);
    auto* idle = new QLabel(tr("No session. Start the program to debug — breakpoints from the "
                               "editor are loaded automatically."), this);
    m_stack->addWidget(idle);

    auto* session = new QWidget(this);
    auto* sessionLayout = new QVBoxLayout(session);
    sessionLayout->setContentsMargins(0, 0, 0, 0);
    sessionLayout->setSpacing(4);

    sessionLayout->addWidget(new QLabel(tr("Call stack:"), session));
    m_stackView = new QTreeWidget(session);
    m_stackView->setHeaderLabels({ tr("#"), tr("Function"), tr("File:Line") });
    m_stackView->setColumnWidth(0, 34);
    m_stackView->setRootIsDecorated(false);
    sessionLayout->addWidget(m_stackView, 2);

    sessionLayout->addWidget(new QLabel(tr("Locals:"), session));
    m_localsView = new QTreeWidget(session);
    m_localsView->setHeaderLabels({ tr("Name"), tr("Value") });
    m_localsView->setRootIsDecorated(false);
    sessionLayout->addWidget(m_localsView, 2);

    auto* evalRow = new QHBoxLayout;
    m_evalInput = new QLineEdit(session);
    m_evalInput->setPlaceholderText(tr("Evaluate expression (Enter)"));
    m_evalBtn = new QToolButton(session);
    m_evalBtn->setText(tr("Evaluate"));
    evalRow->addWidget(m_evalInput, 1);
    evalRow->addWidget(m_evalBtn);
    sessionLayout->addLayout(evalRow);

    m_console = new QPlainTextEdit(session);
    m_console->setReadOnly(true);
    QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    m_console->setFont(mono);
    sessionLayout->addWidget(m_console, 3);

    m_stack->addWidget(session);
    layout->addWidget(m_stack, 1);

    connect(m_startBtn, &QToolButton::clicked, this, &DebugPanel::onStart);
    connect(m_stopBtn, &QToolButton::clicked, this, &DebugPanel::onStop);
    connect(m_continueBtn, &QToolButton::clicked, this, &DebugPanel::onContinue);
    connect(m_pauseBtn, &QToolButton::clicked, this, &DebugPanel::onPause);
    connect(m_stepOverBtn, &QToolButton::clicked, this, &DebugPanel::onStepOver);
    connect(m_stepIntoBtn, &QToolButton::clicked, this, &DebugPanel::onStepInto);
    connect(m_stepOutBtn, &QToolButton::clicked, this, &DebugPanel::onStepOut);
    connect(m_evalBtn, &QToolButton::clicked, this, [this]() {
        if (m_client->isRunning() && !m_evalInput->text().isEmpty())
            m_client->evaluate(m_evalInput->text());
    });
    connect(m_evalInput, &QLineEdit::returnPressed, m_evalBtn, &QToolButton::click);
}

void DebugPanel::addProgramSuggestion(const QString& path)
{
    if (path.isEmpty() || m_program->findText(path) >= 0)
        return;
    m_program->addItem(path);
}

QString DebugPanel::selectedProgram() const
{
    return m_program->currentText().trimmed();
}

void DebugPanel::onStart()
{
    const QString program = selectedProgram();
    if (program.isEmpty()) {
        QMessageBox::information(this, tr("Debug"),
                                 tr("Choose a program to debug (an executable with debug info "
                                    "works best, e.g. built with -g)."));
        return;
    }
    m_console->clear();
    m_stackView->clear();
    m_localsView->clear();
    if (!m_client->start(program, m_args->text().split(QLatin1Char(' '), Qt::SkipEmptyParts),
                         QFileInfo(program).absolutePath())) {
        return;   // error signal already reported
    }
}

void DebugPanel::setSessionActive(bool on)
{
    m_startBtn->setEnabled(!on);
    m_continueBtn->setEnabled(on);
    m_pauseBtn->setEnabled(on);
    m_stepOverBtn->setEnabled(on);
    m_stepIntoBtn->setEnabled(on);
    m_stepOutBtn->setEnabled(on);
    m_stopBtn->setEnabled(on);
    m_stack->setCurrentIndex(on ? 1 : 0);
}

void DebugPanel::onStarted()
{
    setSessionActive(true);
    m_state->setText(tr("Loaded. Press Continue to run."));
    onBreakpointsChanged();
}

void DebugPanel::onRunning()
{
    m_state->setText(tr("Running…"));
}

void DebugPanel::onStopped(const QString& reason, const QString& file, int line)
{
    m_state->setText(tr("Stopped: %1 (%2:%3)").arg(reason, file).arg(line + 1));
    if (!file.isEmpty() && line >= 0)
        emit stoppedAt(file, line);
}

void DebugPanel::onExited(int code)
{
    m_state->setText(tr("Program exited (code %1).").arg(code));
    emit statusMessage(tr("Debug: program exited (%1)").arg(code));
}

void DebugPanel::onStack(const QVector<DebugFrame>& frames)
{
    m_stackView->clear();
    for (const DebugFrame& f : frames) {
        auto* it = new QTreeWidgetItem(m_stackView);
        it->setText(0, QString::number(f.level));
        it->setText(1, f.function);
        it->setText(2, f.file.isEmpty() ? QStringLiteral("(unknown)")
                                        : QStringLiteral("%1:%2").arg(f.file).arg(f.line + 1));
        it->setData(0, Qt::UserRole, f.fullName);
        it->setData(1, Qt::UserRole, f.line);
    }
}

void DebugPanel::onLocals(const QVector<DebugVariable>& vars)
{
    m_localsView->clear();
    for (const DebugVariable& v : vars) {
        auto* it = new QTreeWidgetItem(m_localsView);
        it->setText(0, v.name);
        it->setText(1, v.value);
    }
}

void DebugPanel::onEvaluated(const QString& expression, const QString& value)
{
    Q_UNUSED(expression);
    m_console->appendPlainText(value);
}

void DebugPanel::onConsole(const QString& text)
{
    m_console->appendPlainText(text);
}

void DebugPanel::onError(const QString& text)
{
    m_console->appendPlainText(QStringLiteral("[error] %1").arg(text));
    m_state->setText(text);
}

void DebugPanel::onSessionEnded()
{
    setSessionActive(false);
    m_state->setText(tr("Session ended."));
}

void DebugPanel::onBreakpointsChanged()
{
    if (m_client->isRunning())
        m_client->syncBreakpoints();
}

void DebugPanel::onStop()
{
    m_client->stop();
}

void DebugPanel::onContinue()
{
    m_client->requestContinue();
}

void DebugPanel::onPause()
{
    m_client->requestInterrupt();
}

void DebugPanel::onStepOver()
{
    m_client->requestNext();
}

void DebugPanel::onStepInto()
{
    m_client->requestStep();
}

void DebugPanel::onStepOut()
{
    m_client->requestFinish();
}

}  // namespace cf
