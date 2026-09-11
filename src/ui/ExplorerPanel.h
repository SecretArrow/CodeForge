#pragma once
// Explorer sidebar: workspace toolbar + file tree + context menus + DnD.
#include <QWidget>

class QLineEdit;
class QTreeView;

namespace cf {

class FileTreeModel;
class Workspace;

class ExplorerPanel : public QWidget {
    Q_OBJECT
public:
    explicit ExplorerPanel(Workspace* workspace, QWidget* parent = nullptr);

    void revealPath(const QString& absPath);      // expand ancestors + select
    void applyGitStatus(const QHash<QString, QString>& relState);

signals:
    void requestOpenFile(const QString& path, bool preview);
    void openFolderRequested();

public slots:
    void onWorkspaceChanged(const QString& root);
    void onWorkspaceTreeChanged();                // watcher debounce

private slots:
    void openSelection(bool preview);
    void newFile();
    void newFolder();
    void renameSelected();
    void deleteSelected();
    void duplicateSelected();
    void copySelected();
    void cutSelected();
    void paste();
    void copyPath(bool relative);
    void revealInFileManager();
    void openInTerminal();
    void showProperties();
    void toggleHidden();

private:
    QString selectedPath() const;
    QStringList selectedPaths() const;
    QString selectionDirOrParent() const;   // dir to act on
    void showContextMenu(const QPoint& pos);
    void buildToolbar();

    Workspace* m_workspace;
    FileTreeModel* m_model;
    QTreeView* m_tree;
    QLineEdit* m_filter;
    bool m_previewNext = true;              // single click opens preview, double click pins
};

}  // namespace cf
