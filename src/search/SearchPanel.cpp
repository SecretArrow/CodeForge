#include "search/SearchPanel.h"

#include <QCheckBox>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTextDocument>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "core/DocumentManager.h"
#include "core/Logger.h"
#include "project/Workspace.h"
#include "search/SearchEngine.h"
#include "settings/SettingsManager.h"
#include "ui/Icons.h"

namespace cf {

SearchPanel::SearchPanel(Workspace* workspace, QWidget* parent)
    : QWidget(parent), m_workspace(workspace), m_engine(new SearchEngine(this))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(6);

    // Search row.
    auto* searchRow = new QHBoxLayout();
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Search"));
    m_search->setClearButtonEnabled(true);
    searchRow->addWidget(m_search, 1);
    auto* searchBtn = new QPushButton(tr("Find"), this);
    searchRow->addWidget(searchBtn);
    layout->addLayout(searchRow);

    // Replace row.
    auto* replaceRow = new QHBoxLayout();
    m_replace = new QLineEdit(this);
    m_replace->setPlaceholderText(tr("Replace with"));
    m_replace->setClearButtonEnabled(true);
    replaceRow->addWidget(m_replace, 1);
    m_replaceBtn = new QPushButton(tr("Replace All"), this);
    replaceRow->addWidget(m_replaceBtn);
    layout->addLayout(replaceRow);

    // Options.
    auto* opts = new QHBoxLayout();
    m_case = new QCheckBox(tr("Aa"), this);   m_case->setToolTip(tr("Match case")); m_case->setChecked(true);
    m_word = new QCheckBox(tr("ab|"), this);  m_word->setToolTip(tr("Whole word"));
    m_regex = new QCheckBox(QStringLiteral(".*"), this); m_regex->setToolTip(tr("Regular expression"));
    m_hidden = new QCheckBox(tr("hidden"), this); m_hidden->setToolTip(tr("Include hidden files"));
    opts->addWidget(m_case); opts->addWidget(m_word); opts->addWidget(m_regex); opts->addWidget(m_hidden);
    opts->addStretch(1);
    layout->addLayout(opts);

    // Include/exclude globs.
    auto* grid = new QGridLayout();
    grid->addWidget(new QLabel(tr("Include:"), this), 0, 0);
    m_include = new QLineEdit(this);
    m_include->setPlaceholderText(tr("e.g. src/**,*.cpp"));
    grid->addWidget(m_include, 0, 1);
    grid->addWidget(new QLabel(tr("Exclude:"), this), 1, 0);
    m_exclude = new QLineEdit(this);
    m_exclude->setPlaceholderText(tr("overrides defaults"));
    grid->addWidget(m_exclude, 1, 1);
    layout->addLayout(grid);

    // Results tree.
    m_results = new QTreeWidget(this);
    m_results->setHeaderHidden(true);
    m_results->setUniformRowHeights(true);
    m_results->setSelectionMode(QAbstractItemView::NoSelection);
    layout->addWidget(m_results, 1);

    connect(searchBtn, &QPushButton::clicked, this, &SearchPanel::runSearch);
    connect(m_replaceBtn, &QPushButton::clicked, this, &SearchPanel::replaceAll);
    connect(m_search, &QLineEdit::returnPressed, this, &SearchPanel::runSearch);
    connect(m_engine, &SearchEngine::fileResult, this, &SearchPanel::onFileResult);
    connect(m_engine, &SearchEngine::finished, this, &SearchPanel::onFinished);
    connect(m_results, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item, int col) {
        Q_UNUSED(col);
        const QString path = item->data(0, Qt::UserRole).toString();
        if (path.isEmpty()) return;
        emit resultActivated(path, item->data(0, Qt::UserRole + 1).toInt(), item->data(0, Qt::UserRole + 2).toInt());
    });
    connect(m_engine, &SearchEngine::replacedFile, this, [this](const QString& path, int) {
        // Reload open, clean documents replaced on disk.
        DocumentManager& dm = DocumentManager::instance();
        if (TextDocument* doc = dm.documentByPath(path)) {
            if (!doc->isDirty()) {
                doc->reloadFromDisk();
                doc->snapshotDiskState();
            }
        }
    });
}

void SearchPanel::openAndFocus()
{
    show();
    m_search->setFocus();
    m_search->selectAll();
}

SearchQuery SearchPanel::currentQuery() const
{
    SearchQuery q;
    q.text = m_search->text();
    q.isRegex = m_regex->isChecked();
    q.caseSensitive = m_case->isChecked();
    q.wholeWord = m_word->isChecked();
    q.includeHidden = m_hidden->isChecked() || SettingsManager::instance().getBool(QStringLiteral("search.includeHidden"));
    const QString inc = m_include->text().trimmed();
    if (!inc.isEmpty())
        q.includeGlobs = inc.split(QLatin1Char(','), Qt::SkipEmptyParts);
    const QString exc = m_exclude->text().trimmed();
    if (!exc.isEmpty())
        q.excludeGlobs = exc.split(QLatin1Char(','), Qt::SkipEmptyParts);
    else
        q.excludeGlobs = SettingsManager::instance().get(QStringLiteral("search.excludeGlobs")).toStringList();
    q.maxFileBytes = qint64(SettingsManager::instance().getInt(QStringLiteral("search.maxFileSizeMB"))) * 1024 * 1024;
    return q;
}

void SearchPanel::runSearch()
{
    if (!m_workspace->isOpen() || m_search->text().isEmpty()) return;
    m_results->clear();
    m_currentGroup = nullptr;
    m_totalMatches = 0;
    m_totalFiles = 0;
    m_replaceBtn->setEnabled(false);

    auto* info = new QTreeWidgetItem(m_results, { tr("Searching...") });
    info->setDisabled(true);

    m_engine->start(currentQuery(), m_workspace->rootPath(), m_workspace->indexedFiles());
}

void SearchPanel::onFileResult(const FileResult& fr)
{
    if (m_results->topLevelItemCount() == 1 && m_results->topLevelItem(0)->isDisabled())
        delete m_results->topLevelItem(0);

    m_totalFiles += 1;
    m_totalMatches += fr.hits.size();

    auto* group = new QTreeWidgetItem();
    group->setText(0, QStringLiteral("%1  (%2)").arg(m_workspace->relativePath(fr.path)).arg(fr.hits.size()));
    group->setData(0, Qt::UserRole, fr.path);
    QFont f = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    f.setBold(true);
    group->setFont(0, f);
    m_results->addTopLevelItem(group);
    m_currentGroup = group;

    for (const SearchHit& hit : fr.hits) {
        auto* child = new QTreeWidgetItem(group);
        child->setText(0, QStringLiteral("%1: %2").arg(hit.line + 1, 5).arg(hit.lineText));
        child->setData(0, Qt::UserRole, fr.path);
        child->setData(0, Qt::UserRole + 1, hit.line);
        child->setData(0, Qt::UserRole + 2, hit.colStart);
        child->setFont(0, f);
    }
}

void SearchPanel::onFinished(const SearchStats& stats)
{
    if (m_results->topLevelItemCount() == 1 && m_results->topLevelItem(0)->isDisabled())
        delete m_results->topLevelItem(0);

    const QString summary = stats.cancelled
        ? tr("Cancelled — %1 results in %2 files").arg(m_totalMatches).arg(m_totalFiles)
        : tr("%1 results in %2 files").arg(m_totalMatches).arg(m_totalFiles);
    auto* head = new QTreeWidgetItem(m_results, { summary });
    head->setFirstColumnSpanned(true);
    head->setTextAlignment(0, Qt::AlignCenter);
    m_results->insertTopLevelItem(0, head);
    m_replaceBtn->setEnabled(m_totalMatches > 0 && !m_search->text().isEmpty());
}

void SearchPanel::replaceAll()
{
    if (!m_workspace->isOpen()) return;
    const QString replacement = m_replace->text();
    if (QMessageBox::question(this, tr("Replace All"),
        tr("Replace all matches in the workspace?\n\nOpen modified buffers are updated in place; closed files are written to disk."),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes)
        return;

    // Map of open documents for in-buffer replacement.
    QHash<QString, void*> openDocs;
    for (TextDocument* doc : DocumentManager::instance().documents())
        if (!doc->filePath().isEmpty())
            openDocs.insert(doc->filePath(), doc->document());

    m_engine->replaceAll(currentQuery(), replacement, m_workspace->rootPath(), m_workspace->indexedFiles(), openDocs);
}

}  // namespace cf
