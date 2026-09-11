#pragma once
// Lazy-loading filesystem tree model for the Explorer panel.
// Populates one directory level at a time (fetchMore), keeps node lookup by
// path for git decorations, supports drag & drop moves and clipboard ops.
#include <QAbstractItemModel>
#include <QHash>
#include <QTimer>

class QMimeData;

namespace cf {

class FileTreeModel : public QAbstractItemModel {
    Q_OBJECT
public:
    explicit FileTreeModel(QObject* parent = nullptr);
    ~FileTreeModel() override;

    enum Role { FilePathRole = Qt::UserRole + 1, IsDirRole, GitStateRole };

    void setRoot(const QString& dir);
    void clear();
    QString root() const { return m_root; }

    void setShowHidden(bool show);
    bool showHidden() const { return m_showHidden; }
    void refreshPath(const QString& path);
    void refreshAll();

    void applyGitStatus(const QHash<QString, QString>& relPathState);

    // operations
    QString createFile(const QString& parentDir, const QString& name, QString* err = nullptr);
    QString createFolder(const QString& parentDir, const QString& name, QString* err = nullptr);
    bool renamePath(const QString& oldPath, const QString& newName, QString* err = nullptr);
    bool removePaths(const QStringList& paths, bool permanent, QString* err = nullptr);
    bool duplicatePath(const QString& path, QString* err = nullptr);

    // internal clipboard (cut/copy paths)
    void setClipboard(const QStringList& paths, bool cut);
    bool pasteInto(const QString& targetDir, QString* err = nullptr);

    // drag & drop (external + internal move)
    QStringList mimeTypes() const override;
    QMimeData* mimeData(const QModelIndexList& indexes) const override;
    bool canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) const override;
    bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) override;
    Qt::DropActions supportedDropActions() const override;

    // QAbstractItemModel
    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override { Q_UNUSED(parent); return 1; }
    bool hasChildren(const QModelIndex& parent = QModelIndex()) const override;
    bool canFetchMore(const QModelIndex& parent) const override;
    void fetchMore(const QModelIndex& parent) override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    QModelIndex indexForPath(const QString& absPath) const;   // may be invalid if not populated

signals:
    void errorOccurred(const QString& message);
    void fileCreated(const QString& path);
    void pathRenamed(const QString& oldPath, const QString& newPath);

private:
    struct Node {
        QString name;
        QString path;                 // absolute
        bool isDir = false;
        bool populated = false;
        bool scheduled = false;       // children placeholder created
        Node* parent = nullptr;
        QList<Node*> children;
        QString gitState;             // "", "M", "U", "A", ...
        ~Node() { qDeleteAll(children); }
    };

    Node* nodeForIndex(const QModelIndex& idx) const;
    QModelIndex indexForNode(Node* n) const;
    Node* nodeForPath(const QString& absPath) const;
    void populateNode(Node* n);
    void pruneChildren(Node* n, const QSet<QString>& existingPaths);
    void collectSubtree(Node* n, QList<Node*>& out) const;
    Node* makeNode(Node* parent, const QString& name, bool isDir);

    Node* m_rootNode = nullptr;
    QString m_root;
    bool m_showHidden = false;
    QHash<QString, Node*> m_nodesByPath;
    QStringList m_clipboardPaths;
    bool m_clipboardCut = false;
};

}  // namespace cf
