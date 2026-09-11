#pragma once
// MainWindow: assembles the IDE layout (activity bar, sidebar, editor area,
// bottom panel, status bar), owns the command registry and menus, and wires
// all subsystems together.
#include <QJsonObject>
#include <QMainWindow>
#include <QStackedWidget>
#include <QTimer>

namespace cf {

class BuildManager;
class Breadcrumbs;
class BottomPanel;
class CommandRegistry;
class DocumentManager;
class EditorArea;
class ExplorerPanel;
class ExtensionHost;
class ExtensionsPanel;
class FileWatcher;
class GitClient;
class KeybindManager;
class OutlinePanel;
class QuickOpen;
class RecentManager;
class SearchPanel;
class SessionManager;
class SettingsDialog;
class SettingsManager;
class StatusBar;
class TextDocument;
class ThemeManager;
class Theme;
class Workspace;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

    // QuickOpen / SessionManager accessors
    Workspace& workspace() { return *m_workspace; }
    CommandRegistry& commands() const { return *m_commands; }
    KeybindManager& keybinds() const { return *m_keybinds; }
    TextDocument* activeDocument() const;
    int menuHeight() const { return menuWidget() ? menuWidget()->height() : 0; }

    void openCommandLineTargets(const QStringList& args);   // files/folders passed on the command line
    void openFolderDialog();
    void openFile(const QString& path, bool preview = false, bool addToRecents = true);
    void showRecoveryDialog();

    // session
    QJsonObject sessionState() const;
    bool restoreSession(const QJsonObject& state);

protected:
    void closeEvent(QCloseEvent* e) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onActiveDocChanged(TextDocument* doc);
    void onOpenStateChanged(bool anyOpen);
    void onCursorStats(int line, int col, int selChars, int selLines);
    void handleExternalChangeDialog(TextDocument* doc);
    void applySettingsChanges(const QString& key);

private:
    void buildUi();
    void buildMenus();
    void registerCommands();
    void connectSubsystems();
    void applyThemeNow();
    void updateStatusFor(TextDocument* doc);
    void showFind(bool replace);
    void openRecentPath(const QString& path);

    // members
    QMenu* m_recentMenu = nullptr;
    QHash<QString, QAction*> m_shortcutActions;
    bool m_buildAfterConfigure = false;
    QMetaObject::Connection m_editorCursorConn;

    // subsystems (owned)
    SettingsManager* m_settings = nullptr;
    ThemeManager* m_themes = nullptr;
    CommandRegistry* m_commands = nullptr;
    KeybindManager* m_keybinds = nullptr;
    Workspace* m_workspace = nullptr;
    RecentManager* m_recents = nullptr;
    FileWatcher* m_watcher = nullptr;
    GitClient* m_git = nullptr;
    BuildManager* m_build = nullptr;
    DocumentManager* m_documents = nullptr;
    SessionManager* m_session = nullptr;
    ExtensionHost* m_extensions = nullptr;

    // ui (owned)
    QToolBar* m_activityBar = nullptr;
    QStackedWidget* m_sidebar = nullptr;
    ExplorerPanel* m_explorer = nullptr;
    SearchPanel* m_searchPanel = nullptr;
    QWidget* m_gitPanel = nullptr;
    QWidget* m_buildPanel = nullptr;
    OutlinePanel* m_outline = nullptr;
    ExtensionsPanel* m_extensionsPanel = nullptr;
    EditorArea* m_editorArea = nullptr;
    BottomPanel* m_bottom = nullptr;
    StatusBar* m_status = nullptr;
    Breadcrumbs* m_breadcrumbs = nullptr;
    QuickOpen* m_quickOpen = nullptr;
    QStackedWidget* m_centerStack = nullptr;
    QWidget* m_welcome = nullptr;

    bool m_quitConfirmed = false;
    bool m_restoring = false;
};

}  // namespace cf
