#include "filesystem/FileTreeModel.h"

#include <QDir>
#include <QDirIterator>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QMimeData>
#include <QRegularExpression>
#include <QUrl>

#include "core/Common.h"
#include "core/FileUtils.h"
#include "core/Logger.h"
#include "ui/Icons.h"

namespace cf {

FileTreeModel::FileTreeModel(QObject* parent) : QAbstractItemModel(parent) {}

FileTreeModel::~FileTreeModel()
{
    delete m_rootNode;
}

void FileTreeModel::setRoot(const QString& dir)
{
    beginResetModel();
    delete m_rootNode;
    m_rootNode = nullptr;
    m_nodesByPath.clear();
    m_root = QDir::fromNativeSeparators(QDir(dir).absolutePath());
    if (!m_root.isEmpty()) {
        m_rootNode = new Node();
        m_rootNode->name = m_root;
        m_rootNode->path = m_root;
        m_rootNode->isDir = true;
        m_rootNode->populated = false;
        m_nodesByPath.insert(m_root, m_rootNode);
    }
    endResetModel();
}

void FileTreeModel::clear()
{
    beginResetModel();
    delete m_rootNode;
    m_rootNode = nullptr;
    m_root.clear();
    m_nodesByPath.clear();
    endResetModel();
}

void FileTreeModel::setShowHidden(bool show)
{
    m_showHidden = show;
    refreshAll();
}

FileTreeModel::Node* FileTreeModel::nodeForIndex(const QModelIndex& idx) const
{
    return idx.isValid() ? static_cast<Node*>(idx.internalPointer()) : m_rootNode;
}

QModelIndex FileTreeModel::indexForNode(Node* n) const
{
    if (!n) return QModelIndex();
    if (n == m_rootNode) return QModelIndex();
    const int row = n->parent ? n->parent->children.indexOf(n) : 0;
    if (row < 0) return QModelIndex();
    return createIndex(row, 0, n);
}

FileTreeModel::Node* FileTreeModel::nodeForPath(const QString& absPath) const
{
    return m_nodesByPath.value(QDir::fromNativeSeparators(QFileInfo(absPath).absoluteFilePath()), nullptr);
}

QModelIndex FileTreeModel::indexForPath(const QString& absPath) const
{
    Node* n = nodeForPath(absPath);
    return n ? indexForNode(n) : QModelIndex();
}

QModelIndex FileTreeModel::index(int row, int column, const QModelIndex& parent) const
{
    Node* p = nodeForIndex(parent);
    if (!p || row < 0 || row >= p->children.size() || column != 0) return QModelIndex();
    return createIndex(row, column, p->children.at(row));
}

QModelIndex FileTreeModel::parent(const QModelIndex& child) const
{
    Node* n = nodeForIndex(child);
    if (!n || !n->parent || n->parent == m_rootNode) return QModelIndex();
    return indexForNode(n->parent);
}

int FileTreeModel::rowCount(const QModelIndex& parent) const
{
    Node* n = nodeForIndex(parent);
    if (!n || !n->populated) return 0;
    return n->children.size();
}

bool FileTreeModel::hasChildren(const QModelIndex& parent) const
{
    Node* n = nodeForIndex(parent);
    if (!n) return false;
    if (!n->isDir) return false;
    if (!n->populated) return true;   // assume yes until fetched
    return !n->children.isEmpty();
}

bool FileTreeModel::canFetchMore(const QModelIndex& parent) const
{
    Node* n = nodeForIndex(parent);
    return n && n->isDir && !n->populated;
}

void FileTreeModel::populateNode(Node* n)
{
    if (!n || !n->isDir || n->populated) return;
    QDir dir(n->path);
    const QFileInfoList entries = dir.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot | (m_showHidden ? QDir::Hidden : QDir::Filters()) | QDir::System,
        QDir::DirsFirst | QDir::Name | QDir::LocaleAware);

    for (const QFileInfo& fi : entries) {
        Node* child = makeNode(n, fi.fileName(), fi.isDir());
        Q_UNUSED(child);
    }
    n->populated = true;
}

void FileTreeModel::fetchMore(const QModelIndex& parent)
{
    Node* n = nodeForIndex(parent);
    if (!n || n->populated || !n->isDir) return;

    QDir dir(n->path);
    const QFileInfoList entries = dir.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot | (m_showHidden ? QDir::Hidden : QDir::Filters()) | QDir::System,
        QDir::DirsFirst | QDir::Name | QDir::LocaleAware);

    if (entries.isEmpty()) { n->populated = true; return; }

    const int insertAt = n->children.size();
    beginInsertRows(parent, insertAt, insertAt + entries.size() - 1);
    populateNode(n);
    endInsertRows();
}

void FileTreeModel::pruneChildren(Node* n, const QSet<QString>& existingPaths)
{
    QList<Node*> toRemove;
    for (Node* c : n->children)
        if (!existingPaths.contains(c->path)) toRemove.append(c);
    for (Node* c : toRemove) {
        const int row = n->children.indexOf(c);
        beginRemoveRows(indexForNode(n), row, row);
        n->children.removeAt(row);
        m_nodesByPath.remove(c->path);
        // Prune descendants from lookup map.
        QList<Node*> subtree;
        collectSubtree(c, subtree);
        for (Node* s : subtree) m_nodesByPath.remove(s->path);
        delete c;
        endRemoveRows();
    }
    n->populated = false;   // force refetch
}

void FileTreeModel::collectSubtree(Node* n, QList<Node*>& out) const
{
    out.append(n);
    for (Node* c : n->children) collectSubtree(c, out);
}

void FileTreeModel::refreshPath(const QString& path)
{
    Node* n = nodeForPath(path);
    if (!n || !n->isDir) return;

    const QDir dir(n->path);
    const QFileInfoList entries = dir.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot | (m_showHidden ? QDir::Hidden : QDir::Filters()) | QDir::System,
        QDir::DirsFirst | QDir::Name | QDir::LocaleAware);

    QSet<QString> existing;
    for (const QFileInfo& fi : entries) existing.insert(QDir::fromNativeSeparators(fi.absoluteFilePath()));

    // Remove stale children.
    QList<Node*> stale;
    for (Node* c : n->children)
        if (!existing.contains(c->path)) stale.append(c);
    for (Node* c : stale) {
        const int row = n->children.indexOf(c);
        if (row < 0) continue;
        beginRemoveRows(indexForNode(n), row, row);
        n->children.removeAt(row);
        QList<Node*> subtree;
        collectSubtree(c, subtree);
        for (Node* s : subtree) m_nodesByPath.remove(s->path);
        delete c;
        endRemoveRows();
    }

    // Add new children.
    QList<QFileInfo> fresh;
    for (const QFileInfo& fi : entries) {
        const QString p = QDir::fromNativeSeparators(fi.absoluteFilePath());
        if (!m_nodesByPath.contains(p)) fresh.append(fi);
    }
    if (!fresh.isEmpty()) {
        const int insertAt = n->children.size();
        beginInsertRows(indexForNode(n), insertAt, insertAt + fresh.size() - 1);
        for (const QFileInfo& fi : fresh) makeNode(n, fi.fileName(), fi.isDir());
        endInsertRows();
    }

    // Update git decorations for existing children.
    for (Node* c : n->children) {
        const QModelIndex idx = indexForNode(c);
        if (idx.isValid()) emit dataChanged(idx, idx, { Qt::ForegroundRole, GitStateRole });
    }
}

void FileTreeModel::refreshAll()
{
    if (m_root.isEmpty()) return;
    // Re-populate the visible levels: reset + refetch root.
    beginResetModel();
    if (m_rootNode) {
        qDeleteAll(m_rootNode->children);
        m_rootNode->children.clear();
        m_nodesByPath.clear();
        m_nodesByPath.insert(m_root, m_rootNode);
        m_rootNode->populated = false;
    }
    endResetModel();
}

FileTreeModel::Node* FileTreeModel::makeNode(Node* parent, const QString& name, bool isDir)
{
    auto* n = new Node();
    n->name = name;
    n->path = parent->path + QLatin1Char('/') + name;
    n->isDir = isDir;
    n->parent = parent;
    parent->children.append(n);
    m_nodesByPath.insert(n->path, n);
    return n;
}

QVariant FileTreeModel::data(const QModelIndex& index, int role) const
{
    Node* n = nodeForIndex(index);
    if (!n) return QVariant();

    switch (role) {
    case Qt::DisplayRole:
        return n->name;
    case Qt::DecorationRole: {
        static QFileIconProvider provider;
        const QIcon sys = provider.icon(n->isDir ? QFileIconProvider::Folder : QFileIconProvider::File);
        if (!sys.isNull()) return sys;
        return Icons::icon(n->isDir ? Icons::Name::Folder : Icons::Name::File);
    }
    case Qt::ForegroundRole: {
        if (n->gitState == QLatin1String("M")) return QColor(0xe2, 0xc0, 0x88);
        if (n->gitState == QLatin1String("U") || n->gitState == QLatin1String("?")) return QColor(0x73, 0xc9, 0x90);
        if (n->gitState == QLatin1String("A")) return QColor(0x73, 0xc9, 0x90);
        if (n->gitState == QLatin1String("D")) return QColor(0xe0, 0x7b, 0x7b);
        return QVariant();
    }
    case Qt::ToolTipRole:
        if (n->gitState == QLatin1String("M")) return tr("Modified (git)");
        if (n->gitState == QLatin1String("U") || n->gitState == QLatin1String("?")) return tr("Untracked (git)");
        return n->path;
    case FilePathRole:
        return n->path;
    case IsDirRole:
        return n->isDir;
    case GitStateRole:
        return n->gitState;
    }
    return QVariant();
}

bool FileTreeModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    Node* n = nodeForIndex(index);
    if (!n || role != Qt::EditRole) return false;
    const QString newName = value.toString().trimmed();
    if (newName.isEmpty() || newName == n->name) return false;
    QString err;
    if (!renamePath(n->path, newName, &err)) {
        emit errorOccurred(err);
        return false;
    }
    return true;
}

Qt::ItemFlags FileTreeModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags f = QAbstractItemModel::flags(index);
    Node* n = nodeForIndex(index);
    if (n) {
        f |= Qt::ItemIsDragEnabled;
        f |= Qt::ItemIsEditable;   // inline rename (F2)
        if (n->isDir) f |= Qt::ItemIsDropEnabled;
    }
    return f;
}

void FileTreeModel::applyGitStatus(const QHash<QString, QString>& relPathState)
{
    if (m_root.isEmpty()) return;
    for (auto it = relPathState.constBegin(); it != relPathState.constEnd(); ++it) {
        Node* n = nodeForPath(m_root + QLatin1Char('/') + it.key());
        if (n) {
            n->gitState = it.value();
            const QModelIndex idx = indexForNode(n);
            if (idx.isValid()) emit dataChanged(idx, idx, { Qt::ForegroundRole, GitStateRole });
        }
    }
}

// ---------- operations ----------

QString FileTreeModel::createFile(const QString& parentDir, const QString& name, QString* err)
{
    const QString path = parentDir + QLatin1Char('/') + name;
    if (QFileInfo::exists(path)) {
        if (err) *err = QStringLiteral("'%1' already exists.").arg(name);
        return QString();
    }
    if (!fs::writeAllAtomic(path, QByteArray(), err)) return QString();
    refreshPath(parentDir);
    emit fileCreated(path);
    return path;
}

QString FileTreeModel::createFolder(const QString& parentDir, const QString& name, QString* err)
{
    const QString path = parentDir + QLatin1Char('/') + name;
    if (QFileInfo::exists(path)) {
        if (err) *err = QStringLiteral("'%1' already exists.").arg(name);
        return QString();
    }
    if (!QDir().mkpath(path)) {
        if (err) *err = QStringLiteral("Failed to create folder '%1'.").arg(name);
        return QString();
    }
    refreshPath(parentDir);
    emit fileCreated(path);
    return path;
}

bool FileTreeModel::renamePath(const QString& oldPath, const QString& newName, QString* err)
{
    const QFileInfo info(oldPath);
    const QString newPath = info.absolutePath() + QLatin1Char('/') + newName;
    if (QFileInfo::exists(newPath)) {
        if (err) *err = QStringLiteral("'%1' already exists.").arg(newName);
        return false;
    }
    if (!QDir().rename(oldPath, newPath)) {
        if (err) *err = QStringLiteral("Rename failed (file may be locked).");
        return false;
    }
    // Update node tree.
    Node* n = nodeForPath(oldPath);
    if (n) {
        QList<Node*> subtree;
        collectSubtree(n, subtree);
        for (Node* s : subtree) m_nodesByPath.remove(s->path);
        const QModelIndex idx = indexForNode(n);
        n->name = newName;
        n->path = newPath;
        QList<Node*> subtree2;
        collectSubtree(n, subtree2);
        for (Node* s : subtree2) {
            if (s == n) continue;
            s->path = n->path + s->path.mid(s->path.lastIndexOf(QLatin1Char('/')));
            m_nodesByPath.insert(s->path, s);
        }
        m_nodesByPath.insert(newPath, n);
        if (idx.isValid()) emit dataChanged(idx, idx);
    }
    emit pathRenamed(oldPath, newPath);
    return true;
}

bool FileTreeModel::removePaths(const QStringList& paths, bool permanent, QString* err)
{
    for (const QString& p : paths) {
        if (!fs::removePath(p, permanent, err)) return false;
        refreshPath(QFileInfo(p).absolutePath());
    }
    return true;
}

bool FileTreeModel::duplicatePath(const QString& path, QString* err)
{
    if (!fs::duplicateEntry(path, err)) return false;
    refreshPath(QFileInfo(path).absolutePath());
    return true;
}

// ---------- clipboard ----------

void FileTreeModel::setClipboard(const QStringList& paths, bool cut)
{
    m_clipboardPaths = paths;
    m_clipboardCut = cut;
}

bool FileTreeModel::pasteInto(const QString& targetDir, QString* err)
{
    if (m_clipboardPaths.isEmpty()) return false;
    bool ok = true;
    for (const QString& src : m_clipboardPaths) {
        const QFileInfo srcInfo(src);
        QString target = targetDir + QLatin1Char('/') + srcInfo.fileName();
        if (QFileInfo(target).absoluteFilePath() == QFileInfo(src).absoluteFilePath()) {
            target = fs::uniqueSiblingName(src, srcInfo.isDir());
        }
        if (m_clipboardCut) {
            if (!QDir().rename(src, target)) {
                if (err) *err = QStringLiteral("Move failed: %1").arg(src);
                ok = false;
                continue;
            }
        } else {
            if (srcInfo.isDir()) {
                if (!QDir().mkpath(target)) { if (err) *err = QStringLiteral("Copy failed."); ok = false; continue; }
                // recursive copy
                QDirIterator it(src, QDirIterator::Subdirectories);
                QDir(src).mkpath(target);
                while (it.hasNext()) {
                    const QString from = it.next();
                    const QString rel = QDir(src).relativeFilePath(from);
                    const QString to = target + QLatin1Char('/') + rel;
                    if (QFileInfo(from).isDir()) QDir().mkpath(to);
                    else if (!QFile::copy(from, to)) { if (err) *err = QStringLiteral("Copy failed: %1").arg(from); ok = false; }
                }
            } else {
                if (!QFile::copy(src, target)) { if (err) *err = QStringLiteral("Copy failed: %1").arg(src); ok = false; continue; }
            }
        }
        refreshPath(targetDir);
    }
    if (m_clipboardCut) m_clipboardPaths.clear();
    return ok;
}

// ---------- drag & drop ----------

QStringList FileTreeModel::mimeTypes() const
{
    return { QStringLiteral("text/uri-list") };
}

QMimeData* FileTreeModel::mimeData(const QModelIndexList& indexes) const
{
    auto* mime = new QMimeData();
    QList<QUrl> urls;
    for (const QModelIndex& idx : indexes) {
        Node* n = nodeForIndex(idx);
        if (n) urls.append(QUrl::fromLocalFile(n->path));
    }
    mime->setUrls(urls);
    return mime;
}

bool FileTreeModel::canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) const
{
    Q_UNUSED(action); Q_UNUSED(row); Q_UNUSED(column);
    if (!data->hasUrls()) return false;
    Node* n = nodeForIndex(parent);
    return n && n->isDir;
}

bool FileTreeModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent)
{
    Q_UNUSED(action); Q_UNUSED(row); Q_UNUSED(column);
    if (!data->hasUrls()) return false;
    Node* n = nodeForIndex(parent);
    if (!n || !n->isDir) return false;

    QString err;
    for (const QUrl& url : data->urls()) {
        const QString src = url.toLocalFile();
        if (src.isEmpty()) continue;
        const QFileInfo srcInfo(src);
        QString target = n->path + QLatin1Char('/') + srcInfo.fileName();
        if (QFileInfo(src).absoluteFilePath() == QFileInfo(target).absoluteFilePath()) continue;
        if (!QDir().rename(src, target)) {
            emit errorOccurred(QStringLiteral("Could not move '%1'.").arg(src));
        }
    }
    refreshPath(n->path);
    return true;
}

Qt::DropActions FileTreeModel::supportedDropActions() const
{
    return Qt::MoveAction;
}

}  // namespace cf
