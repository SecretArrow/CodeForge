#include "ui/MainWindow.h"

#include <QAbstractButton>
#include <QApplication>
#include <QActionGroup>
#include <QClipboard>
#include <QCloseEvent>
#include <QDir>
#include <QFileDialog>
#include <QInputDialog>
#include <QJsonArray>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

#include "buildsys/BuildManager.h"
#include "buildsys/BuildPanel.h"
#include "ai/AssistantPanel.h"
#include "autocomplete/CompletionEngine.h"
#include "core/Breakpoints.h"
#include "core/CommandRegistry.h"
#include "core/DocumentManager.h"
#include "core/FileUtils.h"
#include "core/Logger.h"
#include "debug/DebugPanel.h"
#include "debug/DebuggerClient.h"
#include "diff/DiffViewer.h"
#include "editor/Breadcrumbs.h"
#include "editor/CodeEditor.h"
#include "editor/EditorArea.h"
#include "editor/EditorGroup.h"
#include "editor/EditorServices.h"
#include "editormodes/VimEmacsModes.h"
#include "app/UpdateChecker.h"
#include "filesystem/EditorConfig.h"
#include "filesystem/FileWatcher.h"
#include "filesystem/FileTreeModel.h"
#include "git/GitClient.h"
#include "git/GitPanel.h"
#include "lsp/LspManager.h"
#include "project/RecentManager.h"
#include "project/SessionManager.h"
#include "project/Workspace.h"
#include "search/SearchPanel.h"
#include "security/ClipboardGuard.h"
#include "remote/RemotePanel.h"
#include "settings/KeybindManager.h"
#include "settings/SettingsDialog.h"
#include "settings/SettingsManager.h"
#include "snippets/SnippetStore.h"
#include "snippets/SnippetsDialog.h"
#include "syntax/LanguageRegistry.h"
#include "tasks/TaskRunner.h"
#include "terminal/TerminalPane.h"
#include "themes/ThemeManager.h"
#include "tools/DatabasePanel.h"
#include "tools/GrpcPanel.h"
#include "tools/HttpPanel.h"
#include "tools/ProfilerPanel.h"
#include "ui/BottomPanel.h"
#include "ui/ExplorerPanel.h"
#include "ui/ExtensionsPanel.h"
#include "extensions/ExtensionHost.h"
#include "ui/Icons.h"
#include "ui/OutlinePanel.h"
#include "ui/QuickOpen.h"
#include "ui/StatusBar.h"
#include "ui/TodoPanel.h"
#include "ui/WelcomePage.h"

#include <QDesktopServices>
#include <QDialog>
#include <QDockWidget>
#include <QToolTip>
#include <QUrl>

namespace cf {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setObjectName(QStringLiteral("codeforge_mainwindow"));
    setWindowIcon(Icons::icon(Icons::Name::Book));
    setDockNestingEnabled(false);

    // Subsystems.
    m_settings = &SettingsManager::instance();
    m_themes = &ThemeManager::instance();
    m_commands = new CommandRegistry(this);
    m_keybinds = new KeybindManager(m_commands, m_settings, this);
    m_workspace = new Workspace(this);
    m_recents = new RecentManager(this);
    m_recents->load();
    m_watcher = new FileWatcher(this);
    m_git = new GitClient(this);
    m_build = new BuildManager(this);
    m_documents = &DocumentManager::instance();
    m_session = new SessionManager(this);
    m_extensions = new ExtensionHost(this);
    m_lsp = new LspManager(this);
    m_updates = new UpdateChecker(this);
    m_tasks = new TaskRunner(this);
    m_vim = new VimModal(this);
    m_emacs = new EmacsModal(this);

    buildUi();
    buildMenus();
    registerCommands();
    setupEditorServices();

    // Load overrides, theme, apply.
    m_keybinds->load();
    m_themes->loadThemes();
    applyThemeNow();

    connectSubsystems();
    m_settings->load();

    setUnifiedTitleAndToolBarOnMac(false);
    CF_LOG_INFO(QStringLiteral("MainWindow: ready"));
}

void MainWindow::buildUi()
{
    // ---- Center: welcome page + editor area + breadcrumbs ----
    m_editorArea = new EditorArea(this);
    m_breadcrumbs = new Breadcrumbs(this);
    Breadcrumbs::setWorkspace(m_workspace);

    auto* editorColumn = new QWidget(this);
    auto* editorLayout = new QVBoxLayout(editorColumn);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(0);
    editorLayout->addWidget(m_breadcrumbs);
    editorLayout->addWidget(m_editorArea, 1);

    m_welcome = new WelcomePage(m_recents, this);

    m_centerStack = new QStackedWidget(this);
    m_centerStack->addWidget(m_welcome);        // 0
    m_centerStack->addWidget(editorColumn);     // 1
    m_centerStack->setCurrentIndex(0);

    // ---- Bottom panel ----
    m_bottom = new BottomPanel(m_build, this);

    // ---- Vertical split: center / bottom ----
    auto* centerBottom = new QSplitter(Qt::Vertical, this);
    centerBottom->setChildrenCollapsible(false);
    centerBottom->addWidget(m_centerStack);
    centerBottom->addWidget(m_bottom);
    centerBottom->setStretchFactor(0, 4);
    centerBottom->setStretchFactor(1, 1);
    m_bottom->hide();

    // ---- Sidebar pages ----
    m_sidebar = new QStackedWidget(this);
    m_explorer = new ExplorerPanel(m_workspace, this);
    m_searchPanel = new SearchPanel(m_workspace, this);
    m_gitPanel = new GitPanel(m_git, this);
    m_buildPanel = new BuildPanel(m_build, this);
    m_outline = new OutlinePanel(m_editorArea, this);
    m_extensionsPanel = new ExtensionsPanel(m_extensions, this);
    m_todos = new TodoPanel(m_workspace, this);
    m_sidebar->addWidget(m_explorer);        // 0 Explorer
    m_sidebar->addWidget(m_searchPanel);     // 1 Search
    m_sidebar->addWidget(m_gitPanel);        // 2 Source Control
    m_sidebar->addWidget(m_buildPanel);      // 3 Build
    m_sidebar->addWidget(m_outline);         // 4 Outline
    m_sidebar->addWidget(m_extensionsPanel); // 5 Extensions
    m_sidebar->addWidget(m_todos);           // 6 TODO

    // ---- v1.2 panels ----
    m_debug = new DebugPanel(this);
    m_remote = new RemotePanel(this);
    m_http = new HttpPanel(this);
    m_grpc = new GrpcPanel(this);
    m_db = new DatabasePanel(this);
    m_profiler = new ProfilerPanel(this);
    m_sidebar->addWidget(m_debug);           // 7 Debug
    m_sidebar->addWidget(m_remote);          // 8 Remote
    m_bottom->addTab(m_http, Icons::icon(Icons::Name::Globe), tr("HTTP"));
    m_bottom->addTab(m_grpc, Icons::icon(Icons::Name::Api), tr("gRPC"));
    m_bottom->addTab(m_db, Icons::icon(Icons::Name::Database), tr("Database"));
    m_bottom->addTab(m_profiler, Icons::icon(Icons::Name::Gauge), tr("Profiler"));

    // ---- Activity bar ----
    m_activityBar = new QToolBar(this);
    m_activityBar->setOrientation(Qt::Vertical);
    m_activityBar->setMovable(false);
    m_activityBar->setFixedWidth(44);
    m_activityBar->setIconSize(QSize(24, 24));
    addToolBar(Qt::LeftToolBarArea, m_activityBar);

    struct Activity { const char* title; Icons::Name icon; int page; };
    const QList<Activity> activities = {
        { "Explorer", Icons::Name::Folder, 0 },
        { "Search", Icons::Name::Search, 1 },
        { "Source Control", Icons::Name::GitBranch, 2 },
        { "Build", Icons::Name::Play, 3 },
        { "Outline", Icons::Name::Outline, 4 },
        { "Extensions", Icons::Name::Puzzle, 5 },
        { "TODO", Icons::Name::Checklist, 6 },
        { "Debug", Icons::Name::Bug, 7 },
        { "Remote", Icons::Name::Globe, 8 },
    };
    auto* actionGroup = new QActionGroup(this);
    for (const Activity& a : activities) {
        QAction* act = new QAction(Icons::icon(a.icon), tr(a.title), this);
        act->setCheckable(true);
        act->setActionGroup(actionGroup);
        connect(act, &QAction::triggered, this, [this, a, act](bool checked) {
            Q_UNUSED(act);
            if (checked) {
                m_sidebar->setCurrentIndex(a.page);
                m_sidebar->show();
            } else {
                m_sidebar->hide();
            }
        });
        m_activityBar->addAction(act);
    }
    m_activityBar->actions().first()->setChecked(true);

    // Settings gear at the bottom of the activity bar.
    QWidget* stretch = new QWidget(this);
    stretch->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Expanding);
    m_activityBar->addWidget(stretch);
    QAction* gear = new QAction(Icons::icon(Icons::Name::Gear), tr("Settings"), this);
    m_activityBar->addAction(gear);
    gear->setToolTip(tr("Settings"));
    connect(gear, &QAction::triggered, this, [this]() {
        SettingsDialog dialog(m_settings, m_keybinds, m_commands, this);
        dialog.exec();
    });

    // ---- Outer layout: activity bar | sidebar | center ----
    auto* sideSplit = new QSplitter(Qt::Horizontal, this);
    sideSplit->setChildrenCollapsible(false);
    sideSplit->addWidget(m_sidebar);
    sideSplit->addWidget(centerBottom);
    sideSplit->setStretchFactor(0, 0);
    sideSplit->setStretchFactor(1, 1);
    sideSplit->setSizes({ 260, 900 });

    setCentralWidget(sideSplit);

    // ---- Status bar ----
    m_status = new StatusBar(this);
    setStatusBar(m_status);

    // ---- AI Assistant dock (local models) ----
    m_ai = new AssistantPanel(this);
    m_aiDock = new QDockWidget(tr("AI Assistant"), this);
    m_aiDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_aiDock->setWidget(m_ai);
    addDockWidget(Qt::RightDockWidgetArea, m_aiDock);
    m_aiDock->hide();

    // ---- Quick open ----
    m_quickOpen = new QuickOpen(this);

    // Initial state.
    m_sidebar->hide();
    setWindowTitle(QStringLiteral("CodeForge"));
    resize(1280, 800);
}

void MainWindow::buildMenus()
{
    QMenuBar* mb = menuBar();

    // File
    QMenu* file = mb->addMenu(tr("&File"));
    file->addAction(tr("&New File"), QKeySequence::New, this, [this]() {
        m_editorArea->openDocument(m_documents->createUntitled());
    });
    file->addAction(tr("Open &File..."), QKeySequence::Open, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(this, tr("Open File"));
        if (!path.isEmpty()) openFile(path);
    });
    file->addAction(tr("Open &Folder..."), QKeySequence(QStringLiteral("Ctrl+Shift+O")), this, [this]() { openFolderDialog(); });
    file->addSeparator();
    file->addAction(tr("&Save"), QKeySequence::Save, this, [this]() {
        TextDocument* doc = activeDocument();
        if (!doc || doc->isUntitled()) return;
        QString err;
        if (!m_documents->saveDocument(doc, &err) && err.isEmpty() == false)
            QMessageBox::warning(this, tr("Save"), err);
    });
    file->addAction(tr("Save &As..."), QKeySequence::SaveAs, this, [this]() {
        TextDocument* doc = activeDocument();
        if (!doc) return;
        const QString path = QFileDialog::getSaveFileName(this, tr("Save As"), doc->filePath());
        if (path.isEmpty()) return;
        QString err;
        if (!m_documents->saveDocumentAs(doc, path, &err))
            QMessageBox::warning(this, tr("Save As"), err);
    });
    file->addAction(tr("Save A&ll"), QKeySequence(QStringLiteral("Ctrl+K S")), this, [this]() {
        m_documents->saveAllModified();
    });
    file->addSeparator();
    m_recentMenu = file->addMenu(tr("Recent"));
    connect(m_recentMenu, &QMenu::aboutToShow, this, [this]() {
        m_recentMenu->clear();
        for (const RecentManager::Entry& e : m_recents->projects()) {
            QAction* a = m_recentMenu->addAction(QFileInfo(e.path).fileName());
            a->setToolTip(e.path);
            connect(a, &QAction::triggered, this, [this, e]() { openFolderDialog(); openRecentPath(e.path); });
        }
        m_recentMenu->addSeparator();
        m_recentMenu->addAction(tr("Clear"), this, [this]() { m_recents->clearProjects(false); });
    });
    file->addSeparator();
    file->addAction(tr("E&xit"), QKeySequence::Quit, this, &QWidget::close);

    // Edit
    QMenu* edit = mb->addMenu(tr("&Edit"));
    edit->addAction(tr("&Undo"), QKeySequence::Undo, this, [this]() {
        if (m_editorArea->activeGroup() && m_editorArea->activeGroup()->currentEditor()) m_editorArea->activeGroup()->currentEditor()->undo();
    });
    edit->addAction(tr("&Redo"), QKeySequence::Redo, this, [this]() {
        if (m_editorArea->activeGroup() && m_editorArea->activeGroup()->currentEditor()) m_editorArea->activeGroup()->currentEditor()->redo();
    });
    edit->addSeparator();
    edit->addAction(tr("Cu&t"), QKeySequence::Cut, this, [this]() {
        if (m_editorArea->activeGroup() && m_editorArea->activeGroup()->currentEditor()) m_editorArea->activeGroup()->currentEditor()->cut();
    });
    edit->addAction(tr("&Copy"), QKeySequence::Copy, this, [this]() {
        if (m_editorArea->activeGroup() && m_editorArea->activeGroup()->currentEditor()) m_editorArea->activeGroup()->currentEditor()->copy();
    });
    edit->addAction(tr("&Paste"), QKeySequence::Paste, this, [this]() {
        if (m_editorArea->activeGroup() && m_editorArea->activeGroup()->currentEditor()) m_editorArea->activeGroup()->currentEditor()->paste();
    });
    edit->addSeparator();
    edit->addAction(tr("Select &All"), QKeySequence::SelectAll, this, [this]() {
        if (m_editorArea->activeGroup() && m_editorArea->activeGroup()->currentEditor()) m_editorArea->activeGroup()->currentEditor()->selectAll();
    });
    edit->addSeparator();
    edit->addAction(tr("&Find"), QKeySequence::Find, this, [this]() { showFind(false); });
    edit->addAction(tr("&Replace"), QKeySequence(QStringLiteral("Ctrl+H")), this, [this]() { showFind(true); });
    edit->addAction(tr("Find in &Files"), QKeySequence(QStringLiteral("Ctrl+Shift+F")), this, [this]() {
        m_searchPanel->openAndFocus();
    });

    // Selection
    QMenu* selection = mb->addMenu(tr("&Selection"));
    selection->addAction(tr("Select All"), QKeySequence::SelectAll, this, [this]() {
        if (m_editorArea->activeGroup() && m_editorArea->activeGroup()->currentEditor()) m_editorArea->activeGroup()->currentEditor()->selectAll();
    });
    selection->addAction(tr("Expand Selection to Line"), QKeySequence(QStringLiteral("Ctrl+L")), this, [this]() {
        EditorGroup* g = m_editorArea->activeGroup();
        if (!g || !g->currentEditor()) return;
        QTextCursor c = g->currentEditor()->textCursor();
        c.select(QTextCursor::LineUnderCursor);
        g->currentEditor()->setTextCursor(c);
    });
    selection->addSeparator();
    selection->addAction(tr("Add Cursor At Click (Alt+Click)"), this, [this]() {});
    selection->addAction(tr("Add Next Occurrence"), QKeySequence(QStringLiteral("Ctrl+D")), this, [this]() {
        if (CodeEditor* ed = activeEditor()) ed->addNextOccurrence();
    });
    selection->addAction(tr("Skip Occurrence"), QKeySequence(QStringLiteral("Ctrl+K Ctrl+D")), this, [this]() {
        if (CodeEditor* ed = activeEditor()) ed->skipOccurrence();
    });
    selection->addAction(tr("Add Cursor Above"), QKeySequence(QStringLiteral("Ctrl+Alt+Up")), this, [this]() {
        if (CodeEditor* ed = activeEditor()) ed->addCursorAbove();
    });
    selection->addAction(tr("Add Cursor Below"), QKeySequence(QStringLiteral("Ctrl+Alt+Down")), this, [this]() {
        if (CodeEditor* ed = activeEditor()) ed->addCursorBelow();
    });

    // View
    QMenu* view = mb->addMenu(tr("&View"));
    view->addAction(tr("Toggle &Sidebar"), QKeySequence(QStringLiteral("Ctrl+B")), this, [this]() {
        m_sidebar->setVisible(!m_sidebar->isVisible());
    });
    view->addAction(tr("Toggle &Bottom Panel"), QKeySequence(QStringLiteral("Ctrl+J")), this, [this]() {
        m_bottom->setVisible(!m_bottom->isVisible());
    });
    view->addAction(tr("Toggle &Terminal"), QKeySequence(QStringLiteral("Ctrl+`")), this, [this]() {
        if (m_bottom->isVisible() && m_bottom->currentWidget() == m_bottom->terminal()) {
            m_bottom->hide();
        } else {
            if (m_bottom->terminal()->count() == 0) m_bottom->terminal()->newTerminal();
            m_bottom->showTerminal();
            m_bottom->show();
        }
    });
    view->addSeparator();
    view->addAction(tr("&Word Wrap"), QKeySequence(QStringLiteral("Alt+Z")), this, [this]() {
        const bool on = !m_settings->getBool(QStringLiteral("editor.wordWrap"));
        m_settings->set(QStringLiteral("editor.wordWrap"), on);
    });
    view->addSeparator();
    view->addAction(tr("Markdown &Preview"), QKeySequence(QStringLiteral("Ctrl+Shift+V")), this, [this]() {
        if (EditorGroup* g = m_editorArea->activeGroup()) g->toggleMarkdownPreview();
    });
    view->addAction(tr("&Zen Mode"), QKeySequence(QStringLiteral("Ctrl+K Z")), this, [this]() { setZenMode(!m_zen); });
    view->addSeparator();
    QMenu* bookmarkMenu = view->addMenu(tr("&Bookmarks"));
    bookmarkMenu->addAction(tr("Toggle Bookmark"), QKeySequence(QStringLiteral("Ctrl+F2")), this, [this]() {
        if (CodeEditor* ed = activeEditor()) ed->toggleBookmark();
    });
    bookmarkMenu->addAction(tr("Next Bookmark"), QKeySequence(QStringLiteral("F2")), this, [this]() {
        if (CodeEditor* ed = activeEditor()) ed->nextBookmark();
    });
    bookmarkMenu->addAction(tr("Previous Bookmark"), QKeySequence(QStringLiteral("Shift+F2")), this, [this]() {
        if (CodeEditor* ed = activeEditor()) ed->previousBookmark();
    });
    view->addSeparator();
    QMenu* themeMenu = view->addMenu(tr("&Color Theme"));
    for (const Theme& t : m_themes->themes()) {
        QAction* a = themeMenu->addAction(t.name());
        a->setCheckable(true);
        a->setChecked(t.id() == m_settings->getString(QStringLiteral("appearance.theme")));
        connect(a, &QAction::triggered, this, [this, t]() {
            m_themes->setTheme(t.id());
            applyThemeNow();
        });
    }
    view->addAction(tr("&Zoom In"), QKeySequence::ZoomIn, this, [this]() {
        if (m_editorArea->activeGroup() && m_editorArea->activeGroup()->currentEditor())
            m_editorArea->activeGroup()->currentEditor()->increaseFontSize();
    });
    view->addAction(tr("Zoom &Out"), QKeySequence::ZoomOut, this, [this]() {
        if (m_editorArea->activeGroup() && m_editorArea->activeGroup()->currentEditor())
            m_editorArea->activeGroup()->currentEditor()->decreaseFontSize();
    });

    // Go
    QMenu* go = mb->addMenu(tr("&Go"));
    go->addAction(tr("Go to &File..."), QKeySequence(QStringLiteral("Ctrl+P")), this, [this]() { m_quickOpen->openFiles(); });
    go->addAction(tr("Go to &Symbol..."), QKeySequence(QStringLiteral("Ctrl+Shift+O")), this, [this]() { m_quickOpen->openSymbols(); });
    go->addAction(tr("Go to &Line..."), QKeySequence(QStringLiteral("Ctrl+G")), this, [this]() { m_quickOpen->openGotoLine(); });
    go->addAction(tr("Go to &Definition"), QKeySequence(QStringLiteral("F12")), this, [this]() {
        CodeEditor* ed = activeEditor();
        TextDocument* doc = activeDocument();
        if (!ed || !doc || doc->isUntitled()) return;
        QTextCursor c = ed->textCursor();
        QPointer<CodeEditor> guard(ed);
        m_lsp->definition(doc, c.blockNumber(), c.positionInBlock(), [this, guard](const QString& path, int line, int col) {
            if (path.isEmpty() || line < 0) return;
            openFile(path, false, false);
            EditorGroup* g = m_editorArea->activeGroup();
            if (g && g->currentEditor()) g->currentEditor()->gotoLine(line, col);
            Q_UNUSED(guard);
        });
    });

    // Run
    QMenu* run = mb->addMenu(tr("&Run"));
    run->addAction(tr("&Build Project"), QKeySequence(QStringLiteral("Ctrl+Shift+B")), this, [this]() {
        if (!m_build->isCMakeProject()) { QMessageBox::information(this, tr("Build"), tr("No CMakeLists.txt in the workspace root.")); return; }
        m_bottom->showBuild();
        m_bottom->show();
        m_build->configure();
        m_buildAfterConfigure = true;
    });
    run->addAction(tr("&Run Project"), QKeySequence(QStringLiteral("Ctrl+F5")), this, [this]() {
        if (!m_build->isCMakeProject()) return;
        m_bottom->show();
        m_bottom->showBuild();
        const QString exe = m_build->detectExecutable();
        m_build->runExecutable(exe);
    });
    run->addAction(tr("&Stop"), QKeySequence(QStringLiteral("Shift+F5")), this, [this]() { m_build->cancel(); });

    // Terminal
    QMenu* term = mb->addMenu(tr("&Terminal"));
    term->addAction(tr("&New Terminal"), QKeySequence(QStringLiteral("Ctrl+Shift+`")), this, [this]() {
        m_bottom->terminal()->newTerminal();
        m_bottom->showTerminal();
        m_bottom->show();
    });
    term->addAction(tr("&Clear"), this, [this]() { m_bottom->terminal()->clearCurrent(); });
    term->addAction(tr("&Restart"), this, [this]() { m_bottom->terminal()->restartCurrent(); });
    term->addAction(tr("&Kill"), this, [this]() { m_bottom->terminal()->killCurrent(); });

    // Help
    QMenu* help = mb->addMenu(tr("&Help"));
    help->addAction(tr("&Welcome"), QKeySequence(QStringLiteral("Ctrl+Shift+W")), this, [this]() {
        m_centerStack->setCurrentIndex(0);
    });
    help->addAction(tr("Check for &Updates..."), this, [this]() { m_updates->checkNow(); });
    help->addAction(tr("&About CodeForge"), this, [this]() {
        QMessageBox::about(this, tr("About CodeForge"),
            tr("<b>CodeForge %1</b><br>A lightweight native code editor for Windows.<br>"
               "Built with Qt %2 (C++).<br><br>Offline-first. No telemetry. Ever.")
                .arg(QStringLiteral(CF_APP_VERSION), QString::fromLatin1(qVersion())));
    });
}

void MainWindow::registerCommands()
{
    auto add = [this](const QString& id, const QString& category, const QString& title,
                      const QKeySequence& seq, std::function<void()> fn) {
        m_commands->registerCommand(id, category, title, seq, std::move(fn));
    };

    // File commands
    add(QStringLiteral("file.new"), QStringLiteral("File"), QStringLiteral("New File"), QKeySequence(QStringLiteral("Ctrl+N")), [this]() {
        m_editorArea->openDocument(m_documents->createUntitled());
    });
    add(QStringLiteral("file.openFile"), QStringLiteral("File"), QStringLiteral("Open File"), QKeySequence(), [this]() {
        const QString path = QFileDialog::getOpenFileName(this, tr("Open File"));
        if (!path.isEmpty()) openFile(path);
    });
    add(QStringLiteral("file.openFolder"), QStringLiteral("File"), QStringLiteral("Open Folder"), QKeySequence(), [this]() {
        openFolderDialog();
    });
    add(QStringLiteral("file.save"), QStringLiteral("File"), QStringLiteral("Save"), QKeySequence(), [this]() {
        TextDocument* doc = activeDocument();
        if (!doc) return;
        if (doc->isUntitled()) {
            const QString path = QFileDialog::getSaveFileName(this, tr("Save As"));
            if (path.isEmpty()) return;
            QString err;
            m_documents->saveDocumentAs(doc, path, &err);
            return;
        }
        QString err;
        m_documents->saveDocument(doc, &err);
    });
    add(QStringLiteral("file.saveAll"), QStringLiteral("File"), QStringLiteral("Save All"), QKeySequence(), [this]() {
        m_documents->saveAllModified();
    });
    add(QStringLiteral("file.close"), QStringLiteral("File"), QStringLiteral("Close Editor"), QKeySequence(QStringLiteral("Ctrl+W")), [this]() {
        EditorGroup* g = m_editorArea->activeGroup();
        if (g && g->tabCount() > 0) g->closeTab(g->indexOfDoc(g->currentDocument()));
    });
    add(QStringLiteral("file.closeAll"), QStringLiteral("File"), QStringLiteral("Close All Editors"), QKeySequence(), [this]() {
        EditorGroup* g = m_editorArea->activeGroup();
        if (g) g->closeAllTabs();
    });

    // View commands
    add(QStringLiteral("view.sidebar"), QStringLiteral("View"), QStringLiteral("Toggle Sidebar"), QKeySequence(), [this]() {
        m_sidebar->setVisible(!m_sidebar->isVisible());
    });
    add(QStringLiteral("view.panel"), QStringLiteral("View"), QStringLiteral("Toggle Bottom Panel"), QKeySequence(), [this]() {
        m_bottom->setVisible(!m_bottom->isVisible());
    });
    add(QStringLiteral("view.terminal"), QStringLiteral("Terminal"), QStringLiteral("Toggle Terminal"), QKeySequence(), [this]() {
        if (m_bottom->terminal()->count() == 0) m_bottom->terminal()->newTerminal();
        m_bottom->showTerminal();
        m_bottom->setVisible(!m_bottom->isVisible() || m_bottom->currentWidget() != m_bottom->terminal());
    });
    add(QStringLiteral("view.explorer"), QStringLiteral("View"), QStringLiteral("Show Explorer"), QKeySequence(), [this]() {
        m_sidebar->setCurrentIndex(0); m_sidebar->show();
    });
    add(QStringLiteral("view.search"), QStringLiteral("View"), QStringLiteral("Show Search"), QKeySequence(), [this]() {
        m_sidebar->setCurrentIndex(1); m_sidebar->show();
    });
    add(QStringLiteral("view.git"), QStringLiteral("View"), QStringLiteral("Show Source Control"), QKeySequence(), [this]() {
        m_sidebar->setCurrentIndex(2); m_sidebar->show();
    });
    add(QStringLiteral("view.outline"), QStringLiteral("View"), QStringLiteral("Show Outline"), QKeySequence(), [this]() {
        m_sidebar->setCurrentIndex(4); m_sidebar->show();
    });

    // Editor commands
    add(QStringLiteral("editor.splitRight"), QStringLiteral("Editor"), QStringLiteral("Split Editor Right"), QKeySequence(QStringLiteral("Ctrl+\\")), [this]() {
        if (m_editorArea->activeGroup()) m_editorArea->splitGroup(m_editorArea->activeGroup(), Qt::Horizontal, false);
    });
    add(QStringLiteral("editor.splitDown"), QStringLiteral("Editor"), QStringLiteral("Split Editor Down"), QKeySequence(), [this]() {
        if (m_editorArea->activeGroup()) m_editorArea->splitGroup(m_editorArea->activeGroup(), Qt::Vertical, false);
    });
    add(QStringLiteral("editor.nextTab"), QStringLiteral("Editor"), QStringLiteral("Next Tab"), QKeySequence(QStringLiteral("Ctrl+Tab")), [this]() {
        if (m_editorArea->activeGroup()) m_editorArea->activeGroup()->nextTab();
    });
    add(QStringLiteral("editor.prevTab"), QStringLiteral("Editor"), QStringLiteral("Previous Tab"), QKeySequence(QStringLiteral("Ctrl+Shift+Tab")), [this]() {
        if (m_editorArea->activeGroup()) m_editorArea->activeGroup()->previousTab();
    });
    add(QStringLiteral("editor.reopenClosed"), QStringLiteral("Editor"), QStringLiteral("Reopen Closed Editor"), QKeySequence(QStringLiteral("Ctrl+Shift+T")), [this]() {
        // Best effort: reopen last recent file.
        const auto files = m_recents->recentFiles();
        if (!files.isEmpty()) openFile(files.first().path);
    });
    add(QStringLiteral("editor.find"), QStringLiteral("Editor"), QStringLiteral("Find"), QKeySequence(), [this]() { showFind(false); });
    add(QStringLiteral("editor.replace"), QStringLiteral("Editor"), QStringLiteral("Replace"), QKeySequence(), [this]() { showFind(true); });
    add(QStringLiteral("editor.gotoLine"), QStringLiteral("Go"), QStringLiteral("Go to Line"), QKeySequence(), [this]() { m_quickOpen->openGotoLine(); });
    add(QStringLiteral("editor.formatDocument"), QStringLiteral("Editor"), QStringLiteral("Format Document (trim trailing whitespace)"), QKeySequence(QStringLiteral("Shift+Alt+F")), [this]() {
        TextDocument* doc = activeDocument();
        if (!doc) return;
        // Trim + final newline via settings, applied through save path flag.
        m_settings->set(QStringLiteral("files.trimTrailingWhitespace"), true);
        QMessageBox::information(this, tr("Format"),
            tr("Trailing whitespace will be trimmed on save.\n(A dedicated formatter ships via the extension API.)"));
    });

    // Palette + settings + workspace commands
    add(QStringLiteral("workbench.quickOpen"), QStringLiteral("Go"), QStringLiteral("Quick Open File"), QKeySequence(), [this]() { m_quickOpen->openFiles(); });
    add(QStringLiteral("workbench.commandPalette"), QStringLiteral("View"), QStringLiteral("Command Palette"), QKeySequence(), [this]() { m_quickOpen->openCommands(); });
    add(QStringLiteral("workbench.settings"), QStringLiteral("Preferences"), QStringLiteral("Open Settings"), QKeySequence(QStringLiteral("Ctrl+,")), [this]() {
        SettingsDialog dialog(m_settings, m_keybinds, m_commands, this);
        dialog.exec();
    });
    add(QStringLiteral("workbench.theme"), QStringLiteral("Preferences"), QStringLiteral("Change Color Theme"), QKeySequence(QStringLiteral("Ctrl+K Ctrl+T")), [this]() {
        // Cycle to the next theme as a quick switcher; full menu in View.
        const auto themes = m_themes->themes();
        const QString current = m_settings->getString(QStringLiteral("appearance.theme"));
        for (int i = 0; i < themes.size(); ++i) {
            if (themes.at(i).id() == current) {
                const Theme& next = themes.at((i + 1) % themes.size());
                m_themes->setTheme(next.id());
                applyThemeNow();
                break;
            }
        }
    });
    add(QStringLiteral("workbench.reloadWindow"), QStringLiteral("Developer"), QStringLiteral("Reload Window"), QKeySequence(), [this]() {
        const QString root = m_workspace->rootPath();
        if (!root.isEmpty()) {
            m_workspace->openRoot(root);
            m_watcher->setWorkspaceRoot(root);
            m_build->setWorkspaceRoot(root);
        }
    });
    add(QStringLiteral("build.project"), QStringLiteral("Build"), QStringLiteral("Build Project"), QKeySequence(), [this]() {
        if (m_build->isCMakeProject()) { m_bottom->showBuild(); m_bottom->show(); m_build->build(); }
        else QMessageBox::information(this, tr("Build"), tr("No CMakeLists.txt in the workspace root."));
    });
    add(QStringLiteral("build.rebuild"), QStringLiteral("Build"), QStringLiteral("Rebuild Project"), QKeySequence(), [this]() {
        if (m_build->isCMakeProject()) { m_bottom->showBuild(); m_bottom->show(); m_build->rebuild(); }
    });
    add(QStringLiteral("build.clean"), QStringLiteral("Build"), QStringLiteral("Clean Project"), QKeySequence(), [this]() {
        if (m_build->isCMakeProject()) { m_bottom->showBuild(); m_bottom->show(); m_build->clean(); }
    });
    add(QStringLiteral("build.run"), QStringLiteral("Build"), QStringLiteral("Run Project"), QKeySequence(), [this]() {
        if (!m_build->isCMakeProject()) return;
        m_bottom->show();
        m_build->runExecutable(m_build->detectExecutable());
    });

    // ---- v1.1 commands: multi-cursor, bookmarks, markdown, hover, zen, todo, updates ----
    add(QStringLiteral("editor.selectNextOccurrence"), QStringLiteral("Editor"), QStringLiteral("Add Next Occurrence"), QKeySequence(QStringLiteral("Ctrl+D")), [this]() {
        if (CodeEditor* ed = activeEditor()) ed->addNextOccurrence();
    });
    add(QStringLiteral("editor.skipOccurrence"), QStringLiteral("Editor"), QStringLiteral("Skip Occurrence"), QKeySequence(QStringLiteral("Ctrl+K Ctrl+D")), [this]() {
        if (CodeEditor* ed = activeEditor()) ed->skipOccurrence();
    });
    add(QStringLiteral("editor.addCursorAbove"), QStringLiteral("Editor"), QStringLiteral("Add Cursor Above"), QKeySequence(QStringLiteral("Ctrl+Alt+Up")), [this]() {
        if (CodeEditor* ed = activeEditor()) ed->addCursorAbove();
    });
    add(QStringLiteral("editor.addCursorBelow"), QStringLiteral("Editor"), QStringLiteral("Add Cursor Below"), QKeySequence(QStringLiteral("Ctrl+Alt+Down")), [this]() {
        if (CodeEditor* ed = activeEditor()) ed->addCursorBelow();
    });
    add(QStringLiteral("editor.bookmarkToggle"), QStringLiteral("Editor"), QStringLiteral("Toggle Bookmark"), QKeySequence(QStringLiteral("Ctrl+F2")), [this]() {
        if (CodeEditor* ed = activeEditor()) ed->toggleBookmark();
    });
    add(QStringLiteral("editor.bookmarkNext"), QStringLiteral("Editor"), QStringLiteral("Next Bookmark"), QKeySequence(QStringLiteral("F2")), [this]() {
        if (CodeEditor* ed = activeEditor()) ed->nextBookmark();
    });
    add(QStringLiteral("editor.bookmarkPrev"), QStringLiteral("Editor"), QStringLiteral("Previous Bookmark"), QKeySequence(QStringLiteral("Shift+F2")), [this]() {
        if (CodeEditor* ed = activeEditor()) ed->previousBookmark();
    });
    add(QStringLiteral("markdown.openPreview"), QStringLiteral("Markdown"), QStringLiteral("Open Markdown Preview"), QKeySequence(QStringLiteral("Ctrl+Shift+V")), [this]() {
        if (EditorGroup* g = m_editorArea->activeGroup()) g->toggleMarkdownPreview();
    });
    add(QStringLiteral("editor.showHover"), QStringLiteral("Editor"), QStringLiteral("Show Hover (language server)"), QKeySequence(QStringLiteral("Ctrl+K Ctrl+I")), [this]() {
        CodeEditor* ed = activeEditor();
        TextDocument* doc = activeDocument();
        if (!ed || !doc || doc->isUntitled()) return;
        QTextCursor c = ed->textCursor();
        QPointer<CodeEditor> guard(ed);
        m_lsp->hover(doc, c.blockNumber(), c.positionInBlock(), [guard](const QString& html) {
            if (html.isEmpty() || !guard) return;
            QToolTip::showText(guard->cursorRect().bottomRight() + QPoint(0, 8), html, guard);
        });
    });
    add(QStringLiteral("editor.gotoDefinition"), QStringLiteral("Go"), QStringLiteral("Go to Definition"), QKeySequence(QStringLiteral("F12")), [this]() {
        CodeEditor* ed = activeEditor();
        TextDocument* doc = activeDocument();
        if (!ed || !doc || doc->isUntitled()) return;
        QTextCursor c = ed->textCursor();
        m_lsp->definition(doc, c.blockNumber(), c.positionInBlock(), [this](const QString& path, int line, int col) {
            if (path.isEmpty() || line < 0) return;
            openFile(path, false, false);
            EditorGroup* g = m_editorArea->activeGroup();
            if (g && g->currentEditor()) g->currentEditor()->gotoLine(line, col);
        });
    });
    add(QStringLiteral("view.zenMode"), QStringLiteral("View"), QStringLiteral("Toggle Zen Mode"), QKeySequence(QStringLiteral("Ctrl+K Z")), [this]() {
        setZenMode(!m_zen);
    });
    add(QStringLiteral("view.todos"), QStringLiteral("View"), QStringLiteral("Show TODO List"), QKeySequence(), [this]() {
        m_sidebar->setCurrentIndex(6); m_sidebar->show(); m_todos->refresh();
    });
    add(QStringLiteral("workbench.checkForUpdates"), QStringLiteral("Help"), QStringLiteral("Check for Updates"), QKeySequence(), [this]() {
        m_updates->checkNow();
    });

    // ---- v1.2 commands ----
    auto showBottomTab = [this](QWidget* w) {
        m_bottom->show();
        m_bottom->setCurrentWidget(w);
    };
    add(QStringLiteral("view.http"), QStringLiteral("View"), QStringLiteral("HTTP Client"), QKeySequence(), [this, showBottomTab]() { showBottomTab(m_http); m_http->focusUrlEdit(); });
    add(QStringLiteral("view.grpc"), QStringLiteral("View"), QStringLiteral("gRPC Client"), QKeySequence(), [this, showBottomTab]() { showBottomTab(m_grpc); });
    add(QStringLiteral("view.database"), QStringLiteral("View"), QStringLiteral("Database Browser"), QKeySequence(), [this, showBottomTab]() { showBottomTab(m_db); });
    add(QStringLiteral("view.profiler"), QStringLiteral("View"), QStringLiteral("Performance Profiler"), QKeySequence(), [this, showBottomTab]() { showBottomTab(m_profiler); });
    add(QStringLiteral("view.debug"), QStringLiteral("View"), QStringLiteral("Debug Panel"), QKeySequence(QStringLiteral("Ctrl+Shift+D")), [this]() { m_sidebar->setCurrentIndex(7); m_sidebar->show(); });
    add(QStringLiteral("view.remote"), QStringLiteral("View"), QStringLiteral("Remote (SSH)"), QKeySequence(), [this]() { m_sidebar->setCurrentIndex(8); m_sidebar->show(); });
    add(QStringLiteral("view.aiAssistant"), QStringLiteral("View"), QStringLiteral("Toggle AI Assistant"), QKeySequence(QStringLiteral("Ctrl+Shift+A")), [this]() {
        m_aiDock->setVisible(!m_aiDock->isVisible());
    });
    add(QStringLiteral("tasks.run"), QStringLiteral("Tasks"), QStringLiteral("Run Task..."), QKeySequence(), [this]() {
        const QVector<TaskDef> tasks = m_tasks->tasks();
        if (tasks.isEmpty()) {
            if (QMessageBox::question(this, tr("Tasks"),
                    tr("No tasks found. Create a tasks.json template in the workspace?"))
                == QMessageBox::Yes)
                TaskRunner::writeTemplate(m_workspace->rootPath());
            return;
        }
        QStringList labels;
        for (const TaskDef& t : tasks)
            labels << t.label;
        bool ok = false;
        const QString pick = QInputDialog::getItem(this, tr("Run Task"), tr("Task:"), labels, 0, false, &ok);
        if (ok && !pick.isEmpty()) {
            m_bottom->show();
            m_tasks->runTask(pick);
        }
    });
    add(QStringLiteral("tasks.cancel"), QStringLiteral("Tasks"), QStringLiteral("Cancel Running Task"), QKeySequence(), [this]() { m_tasks->cancel(); });
    add(QStringLiteral("snippets.edit"), QStringLiteral("Snippets"), QStringLiteral("Edit Snippets"), QKeySequence(), [this]() {
        SnippetsDialog dialog(this);
        dialog.exec();
    });
    add(QStringLiteral("editor.vimMode"), QStringLiteral("Editor"), QStringLiteral("Toggle Vim Mode"), QKeySequence(), [this]() {
        const bool on = !m_settings->getBool(QStringLiteral("editor.vimMode"));
        m_settings->set(QStringLiteral("editor.emacsMode"), false);
        m_settings->set(QStringLiteral("editor.vimMode"), on);
    });
    add(QStringLiteral("editor.emacsMode"), QStringLiteral("Editor"), QStringLiteral("Toggle Emacs Mode"), QKeySequence(), [this]() {
        const bool on = !m_settings->getBool(QStringLiteral("editor.emacsMode"));
        m_settings->set(QStringLiteral("editor.vimMode"), false);
        m_settings->set(QStringLiteral("editor.emacsMode"), on);
    });
    add(QStringLiteral("file.compare"), QStringLiteral("File"), QStringLiteral("Compare Files..."), QKeySequence(), [this]() {
        const QString a = QFileDialog::getOpenFileName(this, tr("Compare — Side A"));
        if (a.isEmpty()) return;
        const QString b = QFileDialog::getOpenFileName(this, tr("Compare — Side B"));
        if (b.isEmpty()) return;
        QByteArray ba, bb;
        fs::readAll(a, ba);
        fs::readAll(b, bb);
        const enc::Info ea = enc::detect(ba);
        const enc::Info eb = enc::detect(bb);
        showCompareDialog(a, enc::decode(ba, ea), b, enc::decode(bb, eb));
    });
    add(QStringLiteral("file.compareWithHead"), QStringLiteral("File"), QStringLiteral("Compare Active File with Git HEAD"), QKeySequence(), [this]() {
        TextDocument* doc = activeDocument();
        if (!doc || doc->filePath().isEmpty()) return;
        const QString root = m_workspace->rootPath();
        if (root.isEmpty() || !m_git->isRepository()) {
            QMessageBox::information(this, tr("Compare"), tr("Active file is not inside a git repository."));
            return;
        }
        const QString rel = QDir(root).relativeFilePath(doc->filePath());
        QProcess git(this);
        git.setWorkingDirectory(root);
        git.start(QStringLiteral("git"), { QStringLiteral("show"), QStringLiteral("HEAD:") + rel });
        git.waitForFinished(8000);
        const QString headText = git.exitCode() == 0
                                     ? QString::fromUtf8(git.readAllStandardOutput())
                                     : QString();
        QByteArray cur;
        fs::readAll(doc->filePath(), cur);
        const enc::Info ec = enc::detect(cur);
        showCompareDialog(rel + QStringLiteral(" (HEAD)"), headText,
                          rel + QStringLiteral(" (working copy)"), enc::decode(cur, ec));
    });
    add(QStringLiteral("debug.start"), QStringLiteral("Debug"), QStringLiteral("Start Debugging"), QKeySequence(QStringLiteral("F5")), [this]() {
        m_sidebar->setCurrentIndex(7); m_sidebar->show();
        m_debug->onStart();
    });
    add(QStringLiteral("debug.continue"), QStringLiteral("Debug"), QStringLiteral("Continue"), QKeySequence(), [this]() { m_debug->onContinue(); });
    add(QStringLiteral("debug.stop"), QStringLiteral("Debug"), QStringLiteral("Stop Debugging"), QKeySequence(QStringLiteral("Ctrl+Shift+F5")), [this]() { m_debug->onStop(); });
    add(QStringLiteral("debug.stepOver"), QStringLiteral("Debug"), QStringLiteral("Step Over"), QKeySequence(QStringLiteral("F10")), [this]() { m_debug->onStepOver(); });
    add(QStringLiteral("debug.stepInto"), QStringLiteral("Debug"), QStringLiteral("Step Into"), QKeySequence(QStringLiteral("F11")), [this]() { m_debug->onStepInto(); });
    add(QStringLiteral("debug.stepOut"), QStringLiteral("Debug"), QStringLiteral("Step Out"), QKeySequence(QStringLiteral("Shift+F11")), [this]() { m_debug->onStepOut(); });

    // Wire registered shortcuts.
    for (const Command* cmd : m_commands->commands()) {
        const QKeySequence seq = m_keybinds->effective(cmd->id);
        if (seq.isEmpty()) continue;
        QAction* shortcutAction = new QAction(this);
        shortcutAction->setObjectName(cmd->id);
        shortcutAction->setShortcut(seq);
        shortcutAction->setShortcutContext(Qt::WindowShortcut);
        connect(shortcutAction, &QAction::triggered, this, [this, id = cmd->id]() {
            m_commands->execute(id);
        });
        addAction(shortcutAction);
        m_shortcutActions.insert(cmd->id, shortcutAction);
    }
}

void MainWindow::showFind(bool replace)
{
    EditorGroup* g = m_editorArea->activeGroup();
    if (g) g->showFind(replace);
}

void MainWindow::openRecentPath(const QString& path)
{
    Q_UNUSED(path);
}

void MainWindow::applyThemeNow()
{
    const Theme current = m_themes->currentTheme();
    m_themes->applyTheme(current);
}

// ---------------- subsystem wiring ----------------

void MainWindow::connectSubsystems()
{
    // Document manager prompts (UI callbacks keep core headless).
    m_documents->promptSaveBeforeClose = [this](TextDocument* doc) -> DocumentManager::SaveDecision {
        QMessageBox box(this);
        box.setWindowTitle(tr("Unsaved Changes"));
        box.setIcon(QMessageBox::Question);
        box.setText(tr("Do you want to save the changes to '%1'?").arg(doc->displayName()));
        box.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        box.setDefaultButton(QMessageBox::Save);
        const int ret = box.exec();
        switch (ret) {
        case QMessageBox::Save: return DocumentManager::SaveDecision::Save;
        case QMessageBox::Discard: return DocumentManager::SaveDecision::Discard;
        default: return DocumentManager::SaveDecision::Cancel;
        }
    };

    m_documents->promptLargeFile = [this](qint64 bytes) -> DocumentManager::LargeFileDecision {
        QMessageBox box(this);
        box.setWindowTitle(tr("Large File Detected"));
        box.setIcon(QMessageBox::Warning);
        box.setText(tr("This file is %1.\n\nOpening very large files may consume a lot of memory.").arg(formatBytes(bytes)));
        QPushButton* normal = box.addButton(tr("Open Normally"), QMessageBox::AcceptRole);
        QAbstractButton* readOnly = box.addButton(tr("Open Read-Only"), QMessageBox::ActionRole);
        QAbstractButton* cancel = box.addButton(QMessageBox::Cancel);
        Q_UNUSED(normal);
        box.exec();
        if (box.clickedButton() == readOnly) return DocumentManager::LargeFileDecision::ReadOnly;
        if (box.clickedButton() == cancel) return DocumentManager::LargeFileDecision::Cancel;
        return DocumentManager::LargeFileDecision::Normal;
    };

    m_documents->showError = [this](const QString& msg) {
        QMessageBox::critical(this, tr("CodeForge"), msg);
    };

    m_documents->promptExternalChange = [this](TextDocument*) -> int {
        QMessageBox box(this);
        box.setWindowTitle(tr("File Changed Externally"));
        box.setIcon(QMessageBox::Warning);
        box.setText(tr("This file was modified outside the editor.\n\nWhat would you like to do?"));
        QAbstractButton* reload = box.addButton(tr("Reload"), QMessageBox::DestructiveRole);
        QAbstractButton* keep = box.addButton(tr("Keep Changes"), QMessageBox::AcceptRole);
        QAbstractButton* compare = box.addButton(tr("Compare"), QMessageBox::ActionRole);
        Q_UNUSED(keep);
        box.exec();
        if (box.clickedButton() == reload) return 0;
        if (box.clickedButton() == compare) return 2;
        return 1;
    };

    connect(m_documents, &DocumentManager::requestCompareWithDisk, this, [this](TextDocument* doc) {
        if (doc->filePath().isEmpty()) return;
        // Show the on-disk version read-only next to the in-memory version.
        QByteArray bytes;
        if (!fs::readAll(doc->filePath(), bytes)) return;
        const enc::Info e = enc::detect(bytes);
        const QString diskText = enc::decode(bytes, e);
        auto* diskDoc = new TextDocument(this);
        diskDoc->setFilePath(doc->filePath());
        diskDoc->document()->setPlainText(diskText);
        diskDoc->setReadOnly(true);
        EditorGroup* g = m_editorArea->splitGroup(m_editorArea->activeGroup(), Qt::Horizontal, false);
        if (g) g->openDocument(diskDoc);
    });

    // External changes (debounced by FileWatcher).
    connect(m_watcher, &FileWatcher::filesChangedExternally, m_documents, &DocumentManager::checkExternalChanges);
    connect(m_watcher, &FileWatcher::workspaceChanged, this, [this]() {
        m_explorer->onWorkspaceTreeChanged();
    });

    // Editor area -> status bar, breadcrumbs, outline.
    connect(m_editorArea, &EditorArea::activeDocChanged, this, &MainWindow::onActiveDocChanged);
    connect(m_editorArea, &EditorArea::openStateChanged, this, &MainWindow::onOpenStateChanged);

    // Welcome page.
    auto* welcome = qobject_cast<WelcomePage*>(m_welcome);
    connect(welcome, &WelcomePage::openFolderRequested, this, &MainWindow::openFolderDialog);
    connect(welcome, &WelcomePage::openFileRequested, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(this, tr("Open File"));
        if (!path.isEmpty()) openFile(path);
    });
    connect(welcome, &WelcomePage::cloneRequested, this, [this](const QString& url) {
        if (!m_git->hasGit()) {
            QMessageBox::warning(this, tr("Clone"), tr("Git is not available on this system."));
            return;
        }
        const QString target = QFileDialog::getExistingDirectory(this, tr("Choose Parent Folder"));
        if (target.isEmpty()) return;
        const QString dest = target + QLatin1Char('/') + QFileInfo(url).fileName();
        QProcess::startDetached(QStringLiteral("git"), { QStringLiteral("clone"), url, dest }, target);
        m_workspace->openRoot(dest);
        onActiveDocChanged(nullptr);
    });
    connect(welcome, &WelcomePage::openRecentProject, this, [this](const QString& path) {
        if (QFileInfo(path).isDir()) {
            m_workspace->openRoot(path);
        } else {
            openFile(path);
        }
    });
    connect(welcome, &WelcomePage::openRecentFile, this, [this](const QString& path) { openFile(path); });

    // Explorer -> editor.
    connect(m_explorer, &ExplorerPanel::requestOpenFile, this, [this](const QString& path, bool preview) {
        openFile(path, preview);
    });
    connect(m_explorer, &ExplorerPanel::openFolderRequested, this, &MainWindow::openFolderDialog);

    // Search results -> editor.
    connect(m_searchPanel, &SearchPanel::resultActivated, this, [this](const QString& path, int line, int col) {
        openFile(path, false, false);
        EditorGroup* g = m_editorArea->activeGroup();
        if (g && g->currentEditor()) g->currentEditor()->gotoLine(line, col);
    });

    // Workspace lifecycle.
    connect(m_workspace, &Workspace::rootChanged, this, [this](const QString& root) {
        m_explorer->onWorkspaceChanged(root);
        m_watcher->setWorkspaceRoot(root);
        m_build->setWorkspaceRoot(root);
        m_bottom->terminal()->setWorkingDir(root);
        m_git->refresh(root);
        m_lsp->configure(root);
        if (m_settings->getBool(QStringLiteral("files.editorconfig"))) {
            for (TextDocument* doc : m_documents->documents()) {
                if (!doc->isUntitled()) doc->setEditorConfig(EditorConfig::resolve(doc->filePath(), root));
            }
            for (EditorGroup* g : m_editorArea->groups()) {
                for (TextDocument* d : g->documents()) {
                    if (CodeEditor* ed = g->editorForDoc(d)) ed->applyEditorConfig();
                }
            }
        }
        m_todos->refresh();
        if (!root.isEmpty()) {
            m_recents->addProject(root);
            setWindowTitle(QFileInfo(root).fileName() + QStringLiteral(" — CodeForge"));
            m_settings->setWorkspaceRoot(root);
        } else {
            setWindowTitle(QStringLiteral("CodeForge"));
        }
        if (m_gitPanel) qobject_cast<GitPanel*>(m_gitPanel);
    });

    // Git -> UI decorations.
    connect(m_git, &GitClient::statusUpdated, m_explorer, &ExplorerPanel::applyGitStatus);
    connect(m_git, &GitClient::stateChanged, this, [this]() {
        m_status->setBranch(m_git->isRepository() ? m_git->currentBranch() : QString());
    });
    connect(m_git, &GitClient::commandOutput, this, [this](const QString& text) {
        m_bottom->appendOutput(QStringLiteral("Git"), text);
    });

    // Build pipeline: configure triggers build after success.
    connect(m_build, &BuildManager::buildFinished, this, [this](bool ok, int, int) {
        if (ok && m_buildAfterConfigure) {
            m_buildAfterConfigure = false;
            m_build->build();
        }
    });
    connect(m_build, &BuildManager::stateChanged, this, [this](const QString& state) {
        if (state == QStringLiteral("Idle")) m_buildAfterConfigure = false;
    });
    connect(m_bottom, &BottomPanel::problemActivated, this, [this](const QString& file, int line, int col) {
        openFile(file, false, false);
        EditorGroup* g = m_editorArea->activeGroup();
        if (g && g->currentEditor()) g->currentEditor()->gotoLine(line, col);
    });

    // Settings changes -> live apply.
    connect(m_settings, &SettingsManager::changed, this, &MainWindow::applySettingsChanges);

    // Status bar interactions.
    connect(m_status, &StatusBar::positionClicked, this, [this]() { m_quickOpen->openGotoLine(); });
    connect(m_status, &StatusBar::encodingClicked, this, [this]() {
        TextDocument* doc = activeDocument();
        if (!doc) return;
        QStringList labels;
        QVector<enc::Info> infos;
        for (enc::Id id : { enc::Id::Utf8, enc::Id::Utf8Bom, enc::Id::Utf16LE, enc::Id::Utf16BE, enc::Id::Windows1252, enc::Id::Latin1 }) {
            enc::Info info; info.id = id;
            labels << info.label();
            infos << info;
        }
        bool ok = false;
        const QString pick = QInputDialog::getItem(this, tr("Encoding"), tr("Reopen with encoding:"), labels, 0, false, &ok);
        if (!ok) return;
        const int idx = labels.indexOf(pick);
        if (idx < 0) return;
        if (!doc->isDirty()) {
            doc->setEncoding(infos.at(idx));
            doc->reloadFromDisk();
            doc->snapshotDiskState();
            updateStatusFor(doc);
        } else {
            // Save path: keep buffer, change save encoding only.
            doc->setEncoding(infos.at(idx));
            updateStatusFor(doc);
        }
    });
    connect(m_status, &StatusBar::lineEndingsClicked, this, [this]() {
        TextDocument* doc = activeDocument();
        if (!doc) return;
        const QStringList opts = { QStringLiteral("LF"), QStringLiteral("CRLF"), QStringLiteral("CR") };
        bool ok = false;
        const QString pick = QInputDialog::getItem(this, tr("End of Line"), tr("Convert to:"), opts, 0, false, &ok);
        if (!ok) return;
        doc->setLineEndings(lineEndingsFromString(pick));
        doc->markClean();      // conversion applies on save
        doc->document()->setModified(true);   // mark dirty so user saves
        updateStatusFor(doc);
    });
    connect(m_status, &StatusBar::languageClicked, this, [this]() {
        TextDocument* doc = activeDocument();
        if (!doc) return;
        QStringList names;
        for (const Language& l : LanguageRegistry::instance().languages()) names << l.name;
        names << QStringLiteral("Plain Text");
        bool ok = false;
        const QString pick = QInputDialog::getItem(this, tr("Language Mode"), tr("Language:"), names, 0, false, &ok);
        if (!ok) return;
        // Language is derived from extension; changing mode re-highlights manually.
        EditorGroup* g = m_editorArea->activeGroup();
        CodeEditor* ed = g ? g->editorForDoc(doc) : nullptr;
        if (ed) ed->applySettings();
        updateStatusFor(doc);
    });

    // Cursor updates -> status bar + breadcrumbs + outline.
    connect(m_editorArea, &EditorArea::groupActivated, this, [this](EditorGroup*) {
        onActiveDocChanged(activeDocument());
    });

    // Autosave on window deactivate (app loses focus).
    qApp->installEventFilter(this);

    // Clipboard security.
    auto* clipboardGuard = new sec::ClipboardGuard(this);
    clipboardGuard->setClearAfterSeconds(m_settings->getInt(QStringLiteral("security.clipboardClearSeconds")));
    connect(m_settings, &SettingsManager::changed, clipboardGuard, [clipboardGuard](const QString& key, const QVariant&) {
        if (key == QLatin1String("security.clipboardClearSeconds"))
            clipboardGuard->setClearAfterSeconds(SettingsManager::instance().getInt(key));
    });

    // ---- LSP: document lifecycle, diagnostics -> Problems + editor squiggles ----
    connect(m_documents, &DocumentManager::documentOpened, this, [this](TextDocument* doc) {
        m_docPaths.insert(doc->docId(), doc->filePath());
        if (!doc->isUntitled() && m_settings->getBool(QStringLiteral("files.editorconfig")))
            doc->setEditorConfig(EditorConfig::resolve(doc->filePath(), m_workspace->rootPath()));
        m_lsp->handleDocumentOpened(doc);
    });
    connect(m_documents, &DocumentManager::documentClosed, this, [this](const QString& docId) {
        const QString path = m_docPaths.take(docId);
        m_lsp->handleDocumentClosed(docId, path);
    });
    connect(m_lsp, &LspManager::diagnosticsUpdated, this, [this](const QString& path) {
        const QVector<Diagnostic> diags = m_lsp->diagnosticsFor(path);
        m_bottom->updateDiagnostics(path, diags);
        for (EditorGroup* g : m_editorArea->groups()) {
            for (TextDocument* d : g->documents()) {
                if (d->filePath() == path) {
                    if (CodeEditor* ed = g->editorForDoc(d)) ed->setDiagnostics(diags);
                }
            }
        }
    });

    // ---- TODO panel: auto refresh on workspace changes (debounced) ----
    m_todoRefreshTimer.setSingleShot(true);
    m_todoRefreshTimer.setInterval(2500);
    connect(&m_todoRefreshTimer, &QTimer::timeout, m_todos, &TodoPanel::refresh);
    connect(m_documents, &DocumentManager::documentSaved, this, [this](TextDocument* doc) {
        if (doc && m_workspace->isOpen() && m_todos->isVisible()) m_todoRefreshTimer.start();
    });
    connect(m_todos, &TodoPanel::resultActivated, this, [this](const QString& path, int line, int col) {
        openFile(path, false, false);
        EditorGroup* g = m_editorArea->activeGroup();
        if (g && g->currentEditor()) g->currentEditor()->gotoLine(line, col);
    });

    // ---- v1.2: tasks, snippets store, debugger, remote, AI ----
    connect(m_workspace, &Workspace::rootChanged, m_tasks, &TaskRunner::setWorkspaceRoot);
    connect(m_workspace, &Workspace::rootChanged, this, [this](const QString& root) {
        SnippetStore::instance().setWorkspaceRoot(root);
        if (!root.isEmpty())
            m_debug->addProgramSuggestion(m_build->detectExecutable());
    });
    connect(m_tasks, &TaskRunner::taskOutput, this, [this](const QString& label, const QString& line) {
        m_bottom->appendOutput(QStringLiteral("Tasks"),
                               QStringLiteral("[%1] %2").arg(label, line));
    });
    connect(m_tasks, &TaskRunner::taskFinished, this, [this](const QString& label, int code) {
        m_bottom->appendOutput(QStringLiteral("Tasks"),
                               QStringLiteral("[CodeForge] '%1' finished (exit %2)").arg(label).arg(code));
        statusBar()->showMessage(tr("Task '%1' finished (exit %2)").arg(label).arg(code), 5000);
    });
    connect(m_build, &BuildManager::buildFinished, this, [this](bool ok, int, int) {
        if (ok)
            m_debug->addProgramSuggestion(m_build->detectExecutable());
    });
    connect(m_debug, &DebugPanel::stoppedAt, this, [this](const QString& file, int line) {
        openFile(file, false, false);
        EditorGroup* g = m_editorArea->activeGroup();
        if (g && g->currentEditor()) g->currentEditor()->gotoLine(line, 0);
    });
    connect(m_remote, &RemotePanel::remoteFileFetched, this, [this](const QString& remotePath, const QString& local) {
        openFile(local, false, false);
        statusBar()->showMessage(tr("Remote file %1 opened — Ctrl+S pushes it back over SSH")
                                     .arg(remotePath), 6000);
    });
    connect(m_documents, &DocumentManager::documentSaved, this, [this](TextDocument* doc) {
        if (!doc)
            return;
        const RemotePanel::RemoteFile f = m_remote->remoteFileFor(doc->filePath());
        if (f.valid) {
            QString err;
            if (!m_remote->saveBack(f, &err))
                QMessageBox::warning(this, tr("Remote save"), err);
        }
    });
    m_ai->setEditorContextProvider([this](int* selStartLine) -> QString {
        CodeEditor* ed = activeEditor();
        TextDocument* doc = activeDocument();
        if (!ed || !doc)
            return QString();
        QTextCursor c = ed->textCursor();
        if (c.hasSelection()) {
            if (selStartLine)
                *selStartLine = c.blockNumber();
            return c.selectedText().replace(QChar(0x2029), QLatin1Char('\n'));
        }
        if (selStartLine)
            *selStartLine = -1;
        return doc->document()->toPlainText();
    });
    m_ai->setInsertCallback([this](const QString& text) {
        if (CodeEditor* ed = activeEditor())
            ed->textCursor().insertText(text);
    });

    // Vim/Emacs mode feedback.
    connect(m_vim, &VimModal::modeChanged, this, [this](CodeEditor*, const QString& label) {
        statusBar()->showMessage(label.isEmpty() ? QString()
                                                 : QStringLiteral("Vim: %1").arg(label));
    });
    connect(m_vim, &VimModal::searchRequested, this, [this](CodeEditor*, const QString&) {
        showFind(false);
    });
    connect(m_vim, &VimModal::commandRequested, this, [this](CodeEditor*, const QString& raw) {
        const QString cmd = raw.trimmed().startsWith(QLatin1Char(':'))
                                ? raw.trimmed().mid(1)
                                : raw.trimmed();
        if (cmd == QLatin1String("w") || cmd == QLatin1String("wa")) {
            m_documents->saveAllModified();
        } else if (cmd == QLatin1String("q") || cmd == QLatin1String("q!")) {
            close();
        } else if (cmd == QLatin1String("wq") || cmd == QLatin1String("x")) {
            m_documents->saveAllModified();
            close();
        } else {
            bool ok = false;
            const int line = cmd.toInt(&ok);
            if (ok) {
                EditorGroup* g = m_editorArea->activeGroup();
                if (g && g->currentEditor()) g->currentEditor()->gotoLine(line - 1, 0);
            }
        }
    });
    connect(m_emacs, &EmacsModal::commandRequested, this, [this](CodeEditor*, const QString& cmd) {
        if (cmd == QLatin1String("find"))
            showFind(false);
        else if (cmd == QLatin1String("save"))
            m_documents->saveAllModified();
    });
    connect(m_emacs, &EmacsModal::statusMessage, this, [this](CodeEditor*, const QString& msg) {
        statusBar()->showMessage(msg.isEmpty() ? QStringLiteral("Emacs mode off") : msg, msg.isEmpty() ? 2000 : 0);
    });

    setupUpdateChecker();
}

// ---------------- zen mode / helpers ----------------

void MainWindow::setZenMode(bool on)
{
    if (m_zen == on) return;
    m_zen = on;
    if (on) {
        m_zenSidebarVisible = m_sidebar->isVisible();
        m_zenBottomVisible = m_bottom->isVisible();
        m_sidebar->hide();
        m_bottom->hide();
        m_activityBar->hide();
        m_breadcrumbs->hide();
        statusBar()->hide();
    } else {
        m_sidebar->setVisible(m_zenSidebarVisible);
        m_bottom->setVisible(m_zenBottomVisible);
        m_activityBar->show();
        m_breadcrumbs->show();
        statusBar()->show();
    }
}

CodeEditor* MainWindow::activeEditor() const
{
    EditorGroup* g = m_editorArea ? m_editorArea->activeGroup() : nullptr;
    return g ? g->currentEditor() : nullptr;
}

void MainWindow::setupUpdateChecker()
{
    connect(m_updates, &UpdateChecker::checkStarted, this, [this]() {
        statusBar()->showMessage(tr("Checking for updates..."), 4000);
    });
    connect(m_updates, &UpdateChecker::updateAvailable, this, [this](const QString& current, const QString& latest, const QString& url) {
        QMessageBox box(this);
        box.setWindowTitle(tr("Update Available"));
        box.setIcon(QMessageBox::Information);
        box.setText(tr("CodeForge %1 is available (you have %2).").arg(latest, current));
        QAbstractButton* open = box.addButton(tr("Open Releases Page"), QMessageBox::AcceptRole);
        box.addButton(QMessageBox::Ok);
        Q_UNUSED(open);
        box.exec();
        if (box.clickedButton() == open)
            QDesktopServices::openUrl(QUrl(url));
    });
    connect(m_updates, &UpdateChecker::upToDate, this, [this]() {
        statusBar()->showMessage(tr("CodeForge is up to date."), 5000);
    });
    connect(m_updates, &UpdateChecker::checkFailed, this, [this](const QString& err) {
        statusBar()->showMessage(tr("Update check failed: %1").arg(err), 5000);
    });
    // Offline-first: only pings the network when the user opted in.
    QTimer::singleShot(5000, this, [this]() { m_updates->maybeCheckOnStartup(); });
}

// ---------------- status / docs ----------------

TextDocument* MainWindow::activeDocument() const
{
    return m_editorArea && m_editorArea->activeGroup() ? m_editorArea->activeGroup()->currentDocument() : nullptr;
}

void MainWindow::onActiveDocChanged(TextDocument* doc)
{
    m_breadcrumbs->setDocument(doc);
    m_outline->refreshForDocument(doc);
    updateStatusFor(doc);
}

void MainWindow::onOpenStateChanged(bool anyOpen)
{
    m_centerStack->setCurrentIndex(anyOpen ? 1 : 0);
    if (!anyOpen) {
        m_breadcrumbs->setDocument(nullptr);
        m_status->clearFile();
    }
}

void MainWindow::onCursorStats(int line, int col, int selChars, int selLines)
{
    m_status->setCursorInfo(line, col, selChars, selLines);
    m_breadcrumbs->updateCursorLine(line);
}

void MainWindow::updateStatusFor(TextDocument* doc)
{
    if (!doc) {
        m_status->clearFile();
        return;
    }
    const QString shownPath = doc->isUntitled() ? doc->displayName()
                                                : m_workspace->isOpen() ? m_workspace->relativePath(doc->filePath()) : doc->filePath();
    m_status->setFilePath(shownPath);
    m_status->setEncoding(doc->encoding().label());
    m_status->setLineEndings(lineEndingsToString(doc->lineEndings()));
    m_status->setLanguage(doc->languageId().isEmpty() ? QStringLiteral("Plain Text")
                                                      : LanguageRegistry::instance().languageName(doc->languageId()));
    m_status->setFileSize(doc->diskFileSize());
    m_status->setReadOnly(doc->isReadOnly());

    // Hook cursor stats from the current editor.
    EditorGroup* g = m_editorArea->activeGroup();
    CodeEditor* ed = g ? g->editorForDoc(doc) : nullptr;
    if (ed) {
        disconnect(m_editorCursorConn);
        m_editorCursorConn = connect(ed, &CodeEditor::cursorStats, this, &MainWindow::onCursorStats);
        const QTextCursor c = ed->textCursor();
        onCursorStats(c.blockNumber(), c.positionInBlock(), c.selectedText().size(),
                      c.selectedText().count(QChar(u'\u2029')) + (c.hasSelection() ? 1 : 0));
    }
}

void MainWindow::applySettingsChanges(const QString& key)
{
    if (key == QLatin1String("appearance.theme")) {
        m_themes->setTheme(m_settings->getString(QStringLiteral("appearance.theme")));
        applyThemeNow();
    } else if (key == QLatin1String("editor.vimMode") || key == QLatin1String("editor.emacsMode")) {
        applyEditorModes();
    } else if (key.startsWith(QStringLiteral("editor."))) {
        // Reapply editor settings to every open editor.
        for (EditorGroup* g : m_editorArea->groups()) {
            for (TextDocument* d : g->documents()) {
                CodeEditor* ed = g->editorForDoc(d);
                if (ed) ed->applySettings();
            }
        }
    }
}

// ---------------- open helpers ----------------

// ---- v1.2: per-editor services (completion, snippets, modal modes) ----
void MainWindow::setupEditorServices()
{
    EditorServices::instance().addInitHook([this](CodeEditor* ed) {
        if (m_settings->getBool(QStringLiteral("completion.enabled"))) {
            auto* engine = new CompletionEngine(ed, ed);
            const QString lang = LanguageRegistry::instance().detectByPath(
                ed->textDocument() ? ed->textDocument()->filePath() : QString());
            engine->setKeywords(CompletionEngine::keywordsForLanguage(lang));
            engine->setSnippets(SnippetStore::instance().completionItems(lang));
        }
        applyEditorModesTo(ed);
    });
}

void MainWindow::applyEditorModesTo(CodeEditor* ed)
{
    const bool vim = m_settings->getBool(QStringLiteral("editor.vimMode"));
    const bool emacs = m_settings->getBool(QStringLiteral("editor.emacsMode"));
    if (vim)
        m_vim->attachTo(ed);
    else
        m_vim->detachFrom(ed);
    if (emacs)
        m_emacs->attachTo(ed);
    else
        m_emacs->detachFrom(ed);
}

void MainWindow::applyEditorModes()
{
    for (EditorGroup* g : m_editorArea->groups()) {
        for (TextDocument* d : g->documents()) {
            if (CodeEditor* ed = g->editorForDoc(d))
                applyEditorModesTo(ed);
        }
    }
}

void MainWindow::showCompareDialog(const QString& titleA, const QString& textA,
                                   const QString& titleB, const QString& textB)
{
    auto* dlg = new QDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(tr("Compare — %1 / %2").arg(QFileInfo(titleA).fileName(),
                                                    QFileInfo(titleB).fileName()));
    dlg->resize(1100, 700);
    auto* layout = new QVBoxLayout(dlg);
    auto* viewer = new DiffViewer(dlg);
    viewer->setContents(titleA, textA, titleB, textB);
    layout->addWidget(viewer);
    dlg->show();
}

void MainWindow::openFolderDialog()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Open Folder"));
    if (dir.isEmpty()) return;
    m_workspace->openRoot(dir);
    m_centerStack->setCurrentIndex(1);
}

void MainWindow::openFile(const QString& path, bool preview, bool addToRecents)
{
    QString err;
    TextDocument* doc = m_documents->openDocument(path, false, &err);
    if (!doc) {
        if (!err.isEmpty()) QMessageBox::warning(this, tr("Open File"), err);
        return;
    }
    m_editorArea->openDocument(doc, preview);
    m_centerStack->setCurrentIndex(1);
    if (addToRecents) m_recents->addFile(doc->filePath());
    updateStatusFor(doc);
}

void MainWindow::openCommandLineTargets(const QStringList& args)
{
    for (const QString& arg : args) {
        const QFileInfo info(arg);
        if (info.isDir()) m_workspace->openRoot(info.absoluteFilePath());
        else if (info.isFile()) openFile(info.absoluteFilePath());
    }
}

// ---------------- external change dialogs ----------------

void MainWindow::handleExternalChangeDialog(TextDocument* doc)
{
    if (!doc) return;
    const int choice = m_documents->promptExternalChange ? m_documents->promptExternalChange(doc) : 1;
    switch (choice) {
    case 0: doc->reloadFromDisk(); doc->snapshotDiskState(); break;
    case 1: doc->snapshotDiskState(); break;
    case 2: emit m_documents->requestCompareWithDisk(doc); break;
    }
}

void MainWindow::showRecoveryDialog()
{
    const QList<DocumentManager::RecoveryEntry> entries = m_documents->listRecoveryEntries();
    if (entries.isEmpty()) return;

    QStringList names;
    for (const DocumentManager::RecoveryEntry& e : entries) names << (e.title + QStringLiteral("  (%1)")).arg(e.docId);

    QMessageBox box(this);
    box.setWindowTitle(tr("Recovered Files"));
    box.setIcon(QMessageBox::Warning);
    box.setText(tr("CodeForge was closed unexpectedly. Unsaved changes were recovered.\n\nRestore the recovered files?"));
    QAbstractButton* restore = box.addButton(tr("Restore"), QMessageBox::AcceptRole);
    QAbstractButton* discard = box.addButton(tr("Discard"), QMessageBox::DestructiveRole);
    box.addButton(QMessageBox::Cancel);
    box.exec();

    if (box.clickedButton() == restore) {
        for (const DocumentManager::RecoveryEntry& e : entries) {
            TextDocument* doc = m_documents->documentById(e.docId);
            if (!doc) {
                if (!e.path.isEmpty())
                    doc = m_documents->openDocument(e.path);
                else
                    doc = m_documents->createUntitled();
            }
            if (doc && m_documents->restoreRecoveryInto(doc)) {
                m_editorArea->openDocument(doc);
            }
        }
    } else if (box.clickedButton() == discard) {
        for (const DocumentManager::RecoveryEntry& e : entries)
            m_documents->discardRecoveryEntry(e.docId);
    }
}


// ---------------- session ----------------

QJsonObject MainWindow::sessionState() const
{
    QJsonObject state;
    state.insert(QStringLiteral("workspace"), m_workspace->rootPath());
    state.insert(QStringLiteral("editor"), m_editorArea->saveState());
    state.insert(QStringLiteral("geometry"), QString::fromLatin1(saveGeometry().toBase64()));
    state.insert(QStringLiteral("windowState"), QString::fromLatin1(saveState().toBase64()));
    state.insert(QStringLiteral("sidebarVisible"), m_sidebar->isVisible());
    state.insert(QStringLiteral("sidebarPage"), m_sidebar->currentIndex());
    state.insert(QStringLiteral("bottomVisible"), m_bottom->isVisible());
    state.insert(QStringLiteral("theme"), m_settings->getString(QStringLiteral("appearance.theme")));
    return state;
}

bool MainWindow::restoreSession(const QJsonObject& state)
{
    if (state.isEmpty()) return false;
    m_restoring = true;

    const QString workspace = state.value(QStringLiteral("workspace")).toString();
    if (!workspace.isEmpty()) m_workspace->openRoot(workspace);

    const QJsonObject editor = state.value(QStringLiteral("editor")).toObject();
    bool restored = m_editorArea->restoreState(editor);
    if (restored) m_centerStack->setCurrentIndex(1);

    if (state.contains(QStringLiteral("geometry")))
        restoreGeometry(QByteArray::fromBase64(state.value(QStringLiteral("geometry")).toString().toLatin1()));
    if (state.contains(QStringLiteral("windowState")))
        restoreState(QByteArray::fromBase64(state.value(QStringLiteral("windowState")).toString().toLatin1()));

    m_sidebar->setVisible(state.value(QStringLiteral("sidebarVisible")).toBool(true));
    m_sidebar->setCurrentIndex(state.value(QStringLiteral("sidebarPage")).toInt(0));
    m_bottom->setVisible(state.value(QStringLiteral("bottomVisible")).toBool(false));

    onActiveDocChanged(activeDocument());
    m_restoring = false;
    return restored || !workspace.isEmpty();
}

// ---------------- window events ----------------

void MainWindow::closeEvent(QCloseEvent* e)
{
    if (!m_quitConfirmed) {
        // Confirm unsaved documents.
        for (TextDocument* doc : m_documents->documents()) {
            if (!doc->isDirty() || doc->isReadOnly() || doc->isUntitled() == false) {
                if (!doc->isDirty()) continue;
            }
            if (!doc->isDirty()) continue;
            if (m_documents->promptSaveBeforeClose) {
                const DocumentManager::SaveDecision d = m_documents->promptSaveBeforeClose(doc);
                if (d == DocumentManager::SaveDecision::Cancel) { e->ignore(); return; }
                if (d == DocumentManager::SaveDecision::Save && !doc->isUntitled()) {
                    QString err;
                    if (!m_documents->saveDocument(doc, &err)) { e->ignore(); return; }
                }
                if (d == DocumentManager::SaveDecision::Save && doc->isUntitled()) {
                    // Keep unsaved untitled docs in the encrypted recovery store.
                    m_documents->writeRecoverySnapshots();
                }
            }
        }
        m_quitConfirmed = true;
    }

    // Persist session + final recovery for dirty buffers.
    m_session->saveMainWindow(this);
    m_documents->writeRecoverySnapshots();
    m_recents->save();
    m_documents->closeAllDocuments();
    QMainWindow::closeEvent(e);
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::WindowActivate || event->type() == QEvent::WindowDeactivate) {
        if (m_documents->isAutosaveOnFocusChange()) m_documents->autosaveOnFocusChange();
    }
    return QMainWindow::eventFilter(watched, event);
}

}  // namespace cf

