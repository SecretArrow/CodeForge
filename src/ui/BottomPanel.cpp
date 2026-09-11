#include "ui/BottomPanel.h"

#include <QComboBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "buildsys/BuildManager.h"
#include "terminal/TerminalPane.h"
#include "ui/Icons.h"

namespace cf {

BottomPanel::BottomPanel(BuildManager* build, QWidget* parent)
    : QTabWidget(parent)
{
    setDocumentMode(true);
    setObjectName(QStringLiteral("codeforge_bottompanel"));

    // ---- Problems ----
    m_problems = new QTreeWidget(this);
    m_problems->setHeaderLabels({ tr("Severity"), tr("Message"), tr("File"), tr("Line") });
    m_problems->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_problems->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_problems->setRootIsDecorated(false);
    addTab(m_problems, tr("Problems"));
    connect(m_problems, &QTreeWidget::itemActivated, this, &BottomPanel::onProblemActivated);
    connect(m_problems, &QTreeWidget::itemClicked, this, &BottomPanel::onProblemActivated);

    // ---- Output ----
    auto* outputPage = new QWidget(this);
    auto* outLayout = new QVBoxLayout(outputPage);
    outLayout->setContentsMargins(0, 0, 0, 0);
    outLayout->setSpacing(0);
    auto* catRow = new QHBoxLayout();
    catRow->setContentsMargins(4, 2, 4, 2);
    m_outputCategory = new QComboBox(outputPage);
    m_outputCategory->addItems({ QStringLiteral("Build"), QStringLiteral("Git"), QStringLiteral("Run"), QStringLiteral("General") });
    catRow->addWidget(m_outputCategory);
    catRow->addStretch(1);
    outLayout->addLayout(catRow);

    m_output = new QPlainTextEdit(outputPage);
    m_output->setReadOnly(true);
    m_output->setFrameStyle(QFrame::NoFrame);
    m_output->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    outLayout->addWidget(m_output, 1);
    addTab(outputPage, tr("Output"));

    // ---- Terminal ----
    m_terminal = new TerminalPane(this);
    addTab(m_terminal, tr("Terminal"));

    // ---- Build ----
    auto* buildPage = new QWidget(this);
    auto* buildLayout = new QVBoxLayout(buildPage);
    buildLayout->setContentsMargins(0, 0, 0, 0);
    buildLayout->setSpacing(0);
    auto* buildRow = new QHBoxLayout();
    buildRow->setContentsMargins(4, 2, 4, 2);
    auto* summary = new QLabel(tr("CMake project"), buildPage);
    connect(build, &BuildManager::buildFinished, this, [summary, this](bool ok, int errors, int warnings) {
        summary->setText(QStringLiteral("%1  —  %2 errors, %3 warnings")
                             .arg(ok ? tr("Build completed") : tr("Build failed"))
                             .arg(errors)
                             .arg(warnings));
        summary->setStyleSheet(errors > 0 ? QStringLiteral("color: #f14c4c;")
                                          : (warnings > 0 ? QStringLiteral("color: #cca700;") : QStringLiteral("color: #89d185;")));
    });
    connect(build, &BuildManager::stateChanged, this, [summary](const QString& state) {
        if (state != QLatin1String("Idle")) summary->setText(state + QStringLiteral("..."));
        summary->setStyleSheet(QString());
    });
    buildRow->addWidget(summary);
    buildRow->addStretch(1);
    auto* cancelBuild = new QPushButton(tr("Cancel"), buildPage);
    connect(cancelBuild, &QPushButton::clicked, build, &BuildManager::cancel);
    buildRow->addWidget(cancelBuild);
    buildLayout->addLayout(buildRow);

    m_buildOutput = new QPlainTextEdit(buildPage);
    m_buildOutput->setReadOnly(true);
    m_buildOutput->setFrameStyle(QFrame::NoFrame);
    m_buildOutput->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    buildLayout->addWidget(m_buildOutput, 1);
    addTab(buildPage, tr("Build"));

    // ---- Search Results ----
    m_searchResults = new QTreeWidget(this);
    m_searchResults->setHeaderHidden(true);
    m_searchResults->addTopLevelItem(new QTreeWidgetItem({ tr("Run a workspace search (Ctrl+Shift+F) to see results here.") }));
    addTab(m_searchResults, tr("Search Results"));

    connect(build, &BuildManager::outputLine, this, [this](const QString& text) {
        m_buildOutput->moveCursor(QTextCursor::End);
        m_buildOutput->insertPlainText(text);
        m_buildOutput->moveCursor(QTextCursor::End);
    });
    connect(build, &BuildManager::problemsUpdated, this, &BottomPanel::setProblems);
}

void BottomPanel::showProblems() { setCurrentWidget(m_problems); }
void BottomPanel::showOutput()   { setCurrentIndex(1); }
void BottomPanel::showTerminal() { setCurrentWidget(m_terminal); }
void BottomPanel::showBuild()    { setCurrentIndex(3); }

void BottomPanel::showSearchResults(const QString& placeholder)
{
    m_searchResults->clear();
    m_searchResults->addTopLevelItem(new QTreeWidgetItem({ placeholder }));
    setCurrentWidget(m_searchResults);
}

void BottomPanel::appendOutput(const QString& category, const QString& text)
{
    showOutput();
    m_outputCategory->setCurrentText(category);
    m_output->moveCursor(QTextCursor::End);
    m_output->insertPlainText(text);
    m_output->moveCursor(QTextCursor::End);
}

void BottomPanel::setProblems(const QVector<BuildProblem>& problems)
{
    m_problems->clear();
    m_problemCount = problems.size();
    for (const BuildProblem& p : problems) {
        auto* item = new QTreeWidgetItem(m_problems);
        item->setText(0, p.severity == BuildProblem::Error ? tr("error") : tr("warning"));
        item->setText(1, p.code.isEmpty() ? p.message : QStringLiteral("%1: %2").arg(p.code, p.message));
        item->setText(2, p.file);
        item->setText(3, p.line > 0 ? QString::number(p.line) : QString());
        item->setData(0, Qt::UserRole, p.file);
        item->setData(0, Qt::UserRole + 1, p.line - 1);
        item->setData(0, Qt::UserRole + 2, p.column - 1);
        item->setIcon(0, Icons::icon(p.severity == BuildProblem::Error ? Icons::Name::Error : Icons::Name::Warning));
    }
}

void BottomPanel::setBuildSummary(bool ok, int errors, int warnings)
{
    Q_UNUSED(ok); Q_UNUSED(errors); Q_UNUSED(warnings);
}

void BottomPanel::onProblemActivated(QTreeWidgetItem* item, int col)
{
    Q_UNUSED(col);
    if (!item) return;
    const QString file = item->data(0, Qt::UserRole).toString();
    if (file.isEmpty()) return;
    emit problemActivated(file, item->data(0, Qt::UserRole + 1).toInt(), item->data(0, Qt::UserRole + 2).toInt());
}

}  // namespace cf
