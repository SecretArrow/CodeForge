#pragma once
// Integrated terminal: tabbed shell sessions backed by QProcess.
// Shell: PowerShell/pwsh/CMD/Git Bash on Windows, $SHELL elsewhere.
#include <QProcess>
#include <QPlainTextEdit>
#include <QTabWidget>
#include <QLineEdit>
#include <QStringList>

namespace cf {

class TerminalWidget : public QWidget {
    Q_OBJECT
public:
    explicit TerminalWidget(const QString& workingDir, QWidget* parent = nullptr);

    void startShell(const QString& shellId);
    void kill();
    void restart();
    void clear();
    bool isRunning() const { return m_proc && m_proc->state() == QProcess::Running; }
    QString shellId() const { return m_shellId; }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onReadyRead();
    void onFinished(int exitCode, QProcess::ExitStatus status);
    void sendCommand();

private:
    void appendOutput(const QString& text);
    void appendPromptEcho();

    QProcess* m_proc = nullptr;
    QPlainTextEdit* m_output;
    QLineEdit* m_input;
    QString m_shellId;
    QString m_workingDir;
    QStringList m_history;
    int m_historyIndex = -1;
};

class TerminalPane : public QTabWidget {
    Q_OBJECT
public:
    explicit TerminalPane(QWidget* parent = nullptr);

    void setWorkingDir(const QString& dir);
    QString resolveShell() const;

public slots:
    void newTerminal();
    void killCurrent();
    void restartCurrent();
    void clearCurrent();

private:
    QString m_workingDir;
    int m_counter = 0;
};

}  // namespace cf
