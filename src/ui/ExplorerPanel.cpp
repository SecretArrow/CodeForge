#include "ui/ExplorerPanel.h"

#include <QApplication>
#include <QBoxLayout>
#include <QClipboard>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QProcess>
#include <QToolButton>
#include <QTreeView>
#include <QHeaderView>

#include <functional>

#include "core/Common.h"
#include "core/FileUtils.h"
#include "filesystem/FileTreeModel.h"
#include "project/Workspace.h"
#include "settings/SettingsManager.h"
#include "ui/Icons.h"

namespace cf {

ExplorerPanel::ExplorerPanel(Workspace* workspace, QWidget* parent)
    : QWidget(parent), m_workspace(workspace), m_model(new FileTreeModel(this))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 4, 0, 0);
    layout->setSpacing(4);

    buildToolbar();

    m_tree = new QTreeView(this);
    m_tree->setModel(m_model);
    m_tree->setHeaderHidden(true);
    m_tree->setDragEnabled(true);
    m_tree->setAcceptDrops(true);
    m_tree->setDropIndicatorShown(true);
    m_tree->setDragDropMode(QAbstractItemView::DragDrop);
    m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tree->setEditTriggers(QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setIndentation(14);
    m_tree->setExpandsOnDoubleClick(false);
    layout->addWidget(m_tree, 1);

    connect(m_tree, &QTreeView::customContextMenuRequested, this, [this](const QPoint& pos) {
        showContextMenu(m_tree->viewport()->mapToGlobal(pos));
    });
    connect(m_tree, &QTreeView::doubleClicked, this, [this](const QModelIndex& idx) {
        const QString path = idx.data(FileTreeModel::FilePathRole).toString();
        if (idx.data(FileTreeModel::IsDirRole).toBool()) {
            m_tree->expand(idx);
        } else {
            emit requestOpenFile(path, false);
        }
        m_previewNext = false;
    });
    connect(m_tree, &QTreeView::clicked, this, [this](const QModelIndex& idx) {
        const QString path = idx.data(FileTreeModel::FilePathRole).toString();
        if (!idx.data(FileTreeModel::IsDirRole).toBool() && SettingsManager::instance().getBool(QStringLiteral("explorer.previewOnClick")))
            emit requestOpenFile(path, true);
    });
    connect(m_model, &FileTreeModel::errorOccurred, this, [this](const QString& msg) {
        QMessageBox::warning(this, tr("Explorer"), msg);
    });
    connect(m_model, &QAbstractItemModel::modelReset, this, [this]() {
        if (m_model->rowCount() > 0) m_tree->expand(m_model->index(0, 0));
    });

    onWorkspaceChanged(m_workspace->rootPath());
}

void ExplorerPanel::buildToolbar()
{
    auto* barLayout = new QHBoxLayout();
    barLayout->setContentsMargins(6, 0, 6, 0);
    barLayout->setSpacing(2);

    auto addButton = [this, barLayout](Icons::Name icon, const QString& tip, std::function<void()> onClick) {
        QToolButton* b = new QToolButton(this);
        b->setIcon(Icons::icon(icon));
        b->setToolTip(tip);
        b->setAutoRaise(true);
        connect(b, &QToolButton::clicked, this, onClick);
        barLayout->addWidget(b);
    };

    addButton(Icons::Name::FolderOpen, tr("Open Folder"), [this]() { emit openFolderRequested(); });
    addButton(Icons::Name::NewFile, tr("New File"), [this]() { newFile(); });
    addButton(Icons::Name::NewFolder, tr("New Folder"), [this]() { newFolder(); });
    addButton(Icons::Name::Refresh, tr("Refresh"), [this]() { m_model->refreshAll(); });
    addButton(Icons::Name::CollapseAll, tr("Collapse All"), [this]() { m_tree->collapseAll(); });
    addButton(Icons::Name::Filter, tr("Toggle hidden files"), [this]() { toggleHidden(); });

    auto* outer = qobject_cast<QVBoxLayout*>(layout());
    if (outer) outer->insertLayout(0, barLayout);
}

void ExplorerPanel::onWorkspaceChanged(const QString& root)
{
    m_model->setRoot(root);
    if (!root.isEmpty()) {
        m_tree->setRootIndex(QModelIndex());
        m_tree->expand(m_model->index(0, 0));
    }
}

void ExplorerPanel::onWorkspaceTreeChanged()
{
    // Light refresh of visible tree on watcher events.
    m_model->refreshPath(m_model->root());
}

QString ExplorerPanel::selectedPath() const
{
    const QModelIndexList sel = m_tree->selectionModel()->selectedRows();
    return sel.isEmpty() ? m_model->root() : sel.first().data(FileTreeModel::FilePathRole).toString();
}

QStringList ExplorerPanel::selectedPaths() const
{
    QStringList out;
    for (const QModelIndex& idx : m_tree->selectionModel()->selectedRows())
        out.append(idx.data(FileTreeModel::FilePathRole).toString());
    if (out.isEmpty()) out.append(m_model->root());
    return out;
}

QString ExplorerPanel::selectionDirOrParent() const
{
    const QString p = selectedPath();
    QFileInfo info(p);
    return info.isDir() ? p : info.absolutePath();
}

void ExplorerPanel::openSelection(bool preview)
{
    const QString path = selectedPath();
    if (QFileInfo(path).isFile())
        emit requestOpenFile(path, preview);
}

void ExplorerPanel::newFile()
{
    const QString dir = selectionDirOrParent();
    bool ok = false;
    QString name = QInputDialog::getText(this, tr("New File"), tr("File name:"), QLineEdit::Normal, QString(), &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    QString err;
    const QString created = m_model->createFile(dir, name.trimmed(), &err);
    if (created.isEmpty()) { QMessageBox::warning(this, tr("New File"), err); return; }
    emit requestOpenFile(created, false);
}

void ExplorerPanel::newFolder()
{
    const QString dir = selectionDirOrParent();
    bool ok = false;
    QString name = QInputDialog::getText(this, tr("New Folder"), tr("Folder name:"), QLineEdit::Normal, QString(), &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    QString err;
    if (m_model->createFolder(dir, name.trimmed(), &err).isEmpty())
        QMessageBox::warning(this, tr("New Folder"), err);
}

void ExplorerPanel::renameSelected()
{
    const QString path = selectedPath();
    if (path == m_model->root()) return;
    const QFileInfo info(path);
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Rename"), tr("New name:"), QLineEdit::Normal, info.fileName(), &ok);
    if (!ok || name.isEmpty()) return;
    QString err;
    if (!m_model->renamePath(path, name.trimmed(), &err))
        QMessageBox::warning(this, tr("Rename"), err);
}

void ExplorerPanel::deleteSelected()
{
    const QStringList paths = selectedPaths();
    if (paths.size() == 1 && paths.first() == m_model->root()) return;
    const bool multiple = paths.size() > 1;
    const QFileInfo info(paths.first());
    const QString what = multiple ? tr("%1 items").arg(paths.size())
                                  : (info.isDir() ? tr("folder '%1'").arg(info.fileName()) : tr("file '%1'").arg(info.fileName()));
    const auto answer = QMessageBox::question(this, tr("Delete"),
        tr("Are you sure you want to delete %1?\n\n(The file will be moved to the trash when possible.)").arg(what),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (answer != QMessageBox::Yes) return;
    QString err;
    if (!m_model->removePaths(paths, false, &err))
        QMessageBox::warning(this, tr("Delete"), err);
}

void ExplorerPanel::duplicateSelected()
{
    QString err;
    if (!m_model->duplicatePath(selectedPath(), &err))
        QMessageBox::warning(this, tr("Duplicate"), err);
}

void ExplorerPanel::copySelected()
{
    m_model->setClipboard(selectedPaths(), false);
}

void ExplorerPanel::cutSelected()
{
    m_model->setClipboard(selectedPaths(), true);
}

void ExplorerPanel::paste()
{
    QString err;
    if (!m_model->pasteInto(selectionDirOrParent(), &err))
        QMessageBox::warning(this, tr("Paste"), err);
}

void ExplorerPanel::copyPath(bool relative)
{
    QString path = selectedPath();
    if (relative && !m_workspace->rootPath().isEmpty())
        path = m_workspace->relativePath(path);
    QApplication::clipboard()->setText(QDir::toNativeSeparators(path));
}

void ExplorerPanel::revealInFileManager()
{
    fs::revealInFileManager(selectedPath());
}

void ExplorerPanel::openInTerminal()
{
    fs::openTerminalAt(selectionDirOrParent());
}

void ExplorerPanel::showProperties()
{
    const QFileInfo info(selectedPath());
    QMessageBox::information(this, tr("Properties"),
        tr("Path: %1\nType: %2\nSize: %3\nModified: %4\nPermissions: %5")
            .arg(QDir::toNativeSeparators(info.absoluteFilePath()))
            .arg(info.isDir() ? tr("Folder") : tr("File"))
            .arg(formatBytes(info.size()))
            .arg(info.lastModified().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss")))
            .arg(QString::number(int(info.permissions()), 8)));
}

void ExplorerPanel::toggleHidden()
{
    m_model->setShowHidden(!m_model->showHidden());
    SettingsManager::instance().set(QStringLiteral("explorer.showHidden"), m_model->showHidden());
}

void ExplorerPanel::showContextMenu(const QPoint& globalPos)
{
    const QModelIndex idx = m_tree->indexAt(m_tree->viewport()->mapFromGlobal(globalPos));
    if (idx.isValid()) m_tree->selectionModel()->select(idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    const QString path = selectedPath();
    const bool isDir = QFileInfo(path).isDir();
    const bool isRoot = (path == m_model->root());

    QMenu menu(this);
    if (!isRoot) {
        if (!isDir) {
            menu.addAction(Icons::icon(Icons::Name::File), tr("Open"), this, [this]() { openSelection(false); });
            menu.addAction(tr("Open in New Tab"), this, [this]() { openSelection(false); });
            menu.addSeparator();
        }
        menu.addAction(tr("New File..."), this, &ExplorerPanel::newFile);
        menu.addAction(tr("New Folder..."), this, &ExplorerPanel::newFolder);
        menu.addSeparator();
    }
    menu.addAction(tr("Rename"), this, &ExplorerPanel::renameSelected)->setEnabled(!isRoot);
    menu.addAction(Icons::icon(Icons::Name::Trash), tr("Delete"), this, &ExplorerPanel::deleteSelected)->setEnabled(!isRoot);
    menu.addSeparator();
    menu.addAction(tr("Copy"), this, &ExplorerPanel::copySelected);
    menu.addAction(tr("Cut"), this, &ExplorerPanel::cutSelected);
    menu.addAction(tr("Paste"), this, &ExplorerPanel::paste);
    menu.addAction(tr("Duplicate"), this, &ExplorerPanel::duplicateSelected)->setEnabled(!isRoot);
    menu.addSeparator();
    menu.addAction(tr("Copy Path"), this, [this]() { copyPath(false); });
    menu.addAction(tr("Copy Relative Path"), this, [this]() { copyPath(true); });
    menu.addSeparator();
    menu.addAction(tr("Reveal in File Explorer"), this, &ExplorerPanel::revealInFileManager);
    menu.addAction(tr("Open in Terminal"), this, &ExplorerPanel::openInTerminal);
    menu.addSeparator();
    menu.addAction(tr("Properties"), this, &ExplorerPanel::showProperties);
    menu.exec(globalPos);
}

void ExplorerPanel::revealPath(const QString& absPath)
{
    const QModelIndex idx = m_model->indexForPath(absPath);
    if (!idx.isValid()) return;
    // Expand ancestors.
    QModelIndex p = m_model->parent(idx);
    while (p.isValid()) { m_tree->expand(p); p = m_model->parent(p); }
    m_tree->scrollTo(idx, QAbstractItemView::EnsureVisible);
    m_tree->selectionModel()->select(idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

void ExplorerPanel::applyGitStatus(const QHash<QString, QString>& relState)
{
    m_model->applyGitStatus(relState);
}

}  // namespace cf
