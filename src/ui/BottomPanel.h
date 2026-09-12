#pragma once
// Bottom panel with tabs: Problems, Output, Terminal, Build, Search Results.
// Problems merge build (compiler) problems and LSP diagnostics per file.
#include <QHash>
#include <QTabWidget>

#include "buildsys/BuildManager.h"
#include "lsp/LanguageService.h"
#include "ui/Icons.h"

class QPlainTextEdit;
class QTreeWidget;
class QTreeWidgetItem;
class QPushButton;
class QComboBox;

namespace cf {

class TerminalPane;
class BuildManager;

class BottomPanel : public QTabWidget {
    Q_OBJECT
public:
    explicit BottomPanel(BuildManager* build, QWidget* parent = nullptr);

    TerminalPane* terminal() const { return m_terminal; }
    QPlainTextEdit* output() const { return m_output; }
    QPlainTextEdit* buildOutput() const { return m_buildOutput; }

    void showProblems();
    void showOutput();
    void showTerminal();
    void showBuild();
    void appendOutput(const QString& category, const QString& text);
    void setProblems(const QVector<cf::BuildProblem>& problems);
    void updateDiagnostics(const QString& file, const QVector<cf::Diagnostic>& diags);
    void setBuildSummary(bool ok, int errors, int warnings);
    void showSearchResults(const QString& placeholder);

signals:
    void problemActivated(const QString& file, int line, int col);

private slots:
    void onProblemActivated(QTreeWidgetItem* item, int col);

private:
    void rebuildProblems();
    void addProblemRow(const QString& severityText, Icons::Name icon, const QString& code,
                       const QString& message, const QString& file, int line, int column);

    TerminalPane* m_terminal;
    QPlainTextEdit* m_output;
    QPlainTextEdit* m_buildOutput;
    QTreeWidget* m_problems;
    QTreeWidget* m_searchResults;
    QPushButton* m_runStop;
    QComboBox* m_outputCategory;
    QVector<BuildProblem> m_buildProblems;
    QHash<QString, QVector<cf::Diagnostic>> m_lspDiags;
    int m_problemCount = 0;
};

}  // namespace cf
