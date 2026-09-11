#pragma once
// Welcome/start page shown when no documents are open.
#include <QWidget>

class QListWidget;
class QTreeWidgetItem;

namespace cf {

class RecentManager;
class Workspace;

class WelcomePage : public QWidget {
    Q_OBJECT
public:
    explicit WelcomePage(RecentManager* recents, QWidget* parent = nullptr);

    void refreshRecents();

signals:
    void openFolderRequested();
    void openFileRequested();
    void cloneRequested(const QString& url);
    void openRecentProject(const QString& path);
    void openRecentFile(const QString& path);
    void removeRecent(const QString& path);
    void clearRecents();
    void pinRecent(const QString& path, bool pinned);

private slots:
    void onProjectItemMenu(const QPoint& pos);

private:
    RecentManager* m_recents;
    QListWidget* m_projects;
    QListWidget* m_files;
};

}  // namespace cf
