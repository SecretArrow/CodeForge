#pragma once
// DebugPanel: sidebar UI for the GDB/LLDB session (start box, toolbar,
// call stack, locals, console) wired to BreakpointStore for breakpoint sync.
#include <QWidget>

#include "debug/DebuggerClient.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QStackedWidget;
class QTreeWidget;
class QToolButton;

namespace cf {

class DebugPanel : public QWidget {
    Q_OBJECT
public:
    explicit DebugPanel(QWidget* parent = nullptr);

    DebuggerClient* client() const { return m_client; }

    // Program selection helpers (MainWindow fills the combo with build
    // outputs / current file suggestions).
    void addProgramSuggestion(const QString& path);
    QString selectedProgram() const;

signals:
    void statusMessage(const QString& msg);
    void stoppedAt(const QString& file, int line);   // 0-based, for the editor

public slots:
    void onStart();
    void onStop();
    void onContinue();
    void onPause();
    void onStepOver();
    void onStepInto();
    void onStepOut();

private slots:
    void onStarted();
    void onRunning();
    void onStopped(const QString& reason, const QString& file, int line);
    void onExited(int code);
    void onStack(const QVector<cf::DebugFrame>& frames);
    void onLocals(const QVector<cf::DebugVariable>& vars);
    void onEvaluated(const QString& expression, const QString& value);
    void onConsole(const QString& text);
    void onError(const QString& text);
    void onSessionEnded();
    void onBreakpointsChanged();

private:
    void buildUi();
    void setSessionActive(bool on);

    DebuggerClient* m_client = nullptr;
    QComboBox* m_program;
    QLineEdit* m_args;
    QLabel* m_state;
    QStackedWidget* m_stack;        // idle hint / session views
    QTreeWidget* m_stackView;
    QTreeWidget* m_localsView;
    QPlainTextEdit* m_console;
    QToolButton* m_startBtn;
    QToolButton* m_stopBtn;
    QToolButton* m_continueBtn;
    QToolButton* m_pauseBtn;
    QToolButton* m_stepOverBtn;
    QToolButton* m_stepIntoBtn;
    QToolButton* m_stepOutBtn;
    QToolButton* m_evalBtn;
    QLineEdit* m_evalInput;
};

}  // namespace cf
