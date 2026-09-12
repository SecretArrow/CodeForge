#include "ui/TodoPanel.h"

#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "project/Workspace.h"
#include "ui/Icons.h"

namespace cf {

TodoPanel::TodoPanel(Workspace* workspace, QWidget* parent)
    : QWidget(parent), m_workspace(workspace)
{
    setObjectName(QStringLiteral("codeforge_todo"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto* header = new QHBoxLayout();
    m_summary = new QLabel(tr("No workspace open"), this);
    m_summary->setWordWrap(true);
    header->addWidget(m_summary, 1);
    auto* refresh = new QPushButton(Icons::icon(Icons::Name::Refresh), QString(), this);
    refresh->setToolTip(tr("Refresh TODO list"));
    refresh->setFixedWidth(28);
    connect(refresh, &QPushButton::clicked, this, &TodoPanel::refresh);
    header->addWidget(refresh);
    layout->addLayout(header);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setUniformRowHeights(true);
    layout->addWidget(m_tree, 1);

    connect(m_tree, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem* item, int) {
        if (!item || item->parent() == nullptr) return;
        emit resultActivated(item->data(0, Qt::UserRole).toString(),
                             item->data(0, Qt::UserRole + 1).toInt(),
                             item->data(0, Qt::UserRole + 2).toInt());
    });
    connect(m_tree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item, int) {
        if (!item || item->parent() == nullptr) return;
        emit resultActivated(item->data(0, Qt::UserRole).toString(),
                             item->data(0, Qt::UserRole + 1).toInt(),
                             item->data(0, Qt::UserRole + 2).toInt());
    });

    connect(&m_engine, &SearchEngine::fileResult, this, &TodoPanel::onFileResult);
    connect(&m_engine, &SearchEngine::finished, this, &TodoPanel::onFinished);
}

void TodoPanel::clearResults()
{
    m_tree->clear();
    m_total = 0;
    m_files = 0;
}

void TodoPanel::refresh()
{
    if (!m_workspace || !m_workspace->isOpen() || m_engine.isRunning()) return;
    clearResults();
    m_summary->setText(tr("Scanning…"));

    SearchQuery q;
    q.text = QStringLiteral("\\b(TODO|FIXME|HACK|XXX)\\b");
    q.isRegex = true;
    q.caseSensitive = true;
    const QStringList files = m_workspace->indexedFiles();
    m_engine.start(q, m_workspace->rootPath(), files);
}

void TodoPanel::onFileResult(const FileResult& result)
{
    ++m_files;
    m_total += result.hits.size();
    auto* fileItem = new QTreeWidgetItem(m_tree);
    fileItem->setText(0, QStringLiteral("%1  (%2)").arg(QFileInfo(result.path).fileName()).arg(result.hits.size()));
    fileItem->setToolTip(0, result.path);
    fileItem->setIcon(0, Icons::icon(Icons::Name::File));
    for (const SearchHit& hit : result.hits) {
        auto* hitItem = new QTreeWidgetItem(fileItem);
        hitItem->setText(0, QStringLiteral("%1: %2").arg(hit.line + 1).arg(hit.lineText.trimmed()));
        hitItem->setData(0, Qt::UserRole, result.path);
        hitItem->setData(0, Qt::UserRole + 1, hit.line);
        hitItem->setData(0, Qt::UserRole + 2, hit.colStart);
    }
    m_tree->expandItem(fileItem);
}

void TodoPanel::onFinished(const SearchStats& stats)
{
    Q_UNUSED(stats);
    m_summary->setText(tr("%1 markers in %2 files").arg(m_total).arg(m_files));
}

}  // namespace cf
