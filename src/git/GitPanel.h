#pragma once
// Source Control sidebar: changes list, commit box, git actions.
#include <QWidget>

class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QTreeWidget;
class QTreeWidgetItem;
class QPushButton;

namespace cf {

class GitClient;

class GitPanel : public QWidget {
    Q_OBJECT
public:
    explicit GitPanel(GitClient* git, QWidget* parent = nullptr);

signals:
    void openDiffRequested(const QString& path, const QString& diffText);

private slots:
    void refreshView();
    void onStateChanged();
    void commit();

private:
    enum class Section { Staged, Unstaged, Untracked };

    void stageItem(QTreeWidgetItem* item, bool unstage);
    void discardItem(QTreeWidgetItem* item);

    GitClient* m_git;
    QComboBox* m_branchBox;
    QLineEdit* m_summary;
    QPlainTextEdit* m_message;
    QTreeWidget* m_changes;
    QPushButton* m_commitBtn;
    QHash<QString, QString> m_status;
    bool m_updatingBranches = false;
};

}  // namespace cf
