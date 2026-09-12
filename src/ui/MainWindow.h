#pragma once
// MainWindow: assembles the IDE layout (activity bar, sidebar, editor area,
// bottom panel, status bar), owns the command registry and menus, and wires
// all subsystems together.
#include <QJsonObject>
#include <QMainWindow>
#include <QStackedWidget>
#include <QTimer>

namespace cf {

class AssistantPanel;
class BuildManager;
class Breadcrumbs;
class BottomPanel;
class CodeEditor;
class CommandRegistry;
class DatabasePanel;
class DebugPanel;
class DocumentManager;
class EditorArea;
class EmacsModal;
class ExplorerPanel;
class ExtensionHost;
class ExtensionsPanel;
class FileWatcher;
class GitClient;
class GrpcPanel;
class HttpPanel;
class KeybindManager;
class LspManager;
class OutlinePanel;
class ProfilerPanel;
class QuickOpen;
class RecentManager;
class RemotePanel;
class SearchPanel;
class SessionManager;
class SettingsDialog;
class SettingsManager;
class StatusBar;
class TaskRunner;
class TextDocument;
class ThemeManager;
class Theme;
class TodoPanel;
class UpdateChecker;
class VimModal;
class Workspace;
class QDockWidget;

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
    void setZenMode(bool on);

    // v1.2 integrations
    void setupEditorServices();
    void applyEditorModes();
    void applyEditorModesTo(CodeEditor* ed);
    void showCompareDialog(const QString& titleA, const QString& textA,
                           const QString& titleB, const QString& textB);

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
    CodeEditor* activeEditor() const;
    void setupUpdateChecker();

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
    LspManager* m_lsp = nullptr;
    UpdateChecker* m_updates = nullptr;
    TaskRunner* m_tasks = nullptr;
    VimModal* m_vim = nullptr;
    EmacsModal* m_emacs = nullptr;

    // ui (owned)
    QToolBar* m_activityBar = nullptr;
    QStackedWidget* m_sidebar = nullptr;
    ExplorerPanel* m_explorer = nullptr;
    SearchPanel* m_searchPanel = nullptr;
    QWidget* m_gitPanel = nullptr;
    QWidget* m_buildPanel = nullptr;
    OutlinePanel* m_outline = nullptr;
    ExtensionsPanel* m_extensionsPanel = nullptr;
    TodoPanel* m_todos = nullptr;
    EditorArea* m_editorArea = nullptr;
    BottomPanel* m_bottom = nullptr;
    StatusBar* m_status = nullptr;
    Breadcrumbs* m_breadcrumbs = nullptr;
    QuickOpen* m_quickOpen = nullptr;
    QStackedWidget* m_centerStack = nullptr;
    QWidget* m_welcome = nullptr;

    // v1.2 panels
    DebugPanel* m_debug = nullptr;
    RemotePanel* m_remote = nullptr;
    HttpPanel* m_http = nullptr;
    GrpcPanel* m_grpc = nullptr;
    DatabasePanel* m_db = nullptr;
    ProfilerPanel* m_profiler = nullptr;
    AssistantPanel* m_ai = nullptr;
    QDockWidget* m_aiDock = nullptr;

    // zen mode + misc
    bool m_quitConfirmed = false;
    bool m_restoring = false;
    bool m_zen = false;
    bool m_zenSidebarVisible = false;
    bool m_zenBottomVisible = false;
    QTimer m_todoRefreshTimer;
    QHash<QString, QString> m_docPaths;   // docId -> path (for LSP close)
};

}  // namespace cf
