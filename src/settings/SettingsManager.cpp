#include "settings/SettingsManager.h"

#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>

#include "core/AppPaths.h"
#include "core/FileUtils.h"
#include "core/Logger.h"

namespace cf {

SettingsManager& SettingsManager::instance()
{
    static SettingsManager s;
    return s;
}

SettingsManager::SettingsManager(QObject* parent) : QObject(parent)
{
    buildSchema();
}

void SettingsManager::buildSchema()
{
    using T = SettingDef::Type;
    m_defs = {
        // ---- Appearance ----
        { QStringLiteral("appearance.theme"), QStringLiteral("Appearance"), QStringLiteral("Color theme"),
          QStringLiteral("UI and editor color scheme."), T::String, QStringLiteral("dark-plus"), {} },
        { QStringLiteral("appearance.density"), QStringLiteral("Appearance"), QStringLiteral("UI density"),
          QStringLiteral("Compact reduces paddings in lists and toolbars."), T::Enum, QStringLiteral("compact"),
          { { QStringLiteral("Compact"), QStringLiteral("compact") }, { QStringLiteral("Comfortable"), QStringLiteral("comfortable") } } },
        { QStringLiteral("appearance.showActivityBar"), QStringLiteral("Appearance"), QStringLiteral("Show activity bar"),
          QString(), T::Bool, true, {} },

        // ---- Editor ----
        { QStringLiteral("editor.fontFamily"), QStringLiteral("Editor"), QStringLiteral("Font family"),
          QStringLiteral("Monospace font used in the editor."), T::String, QStringLiteral("Consolas"), {} },
        { QStringLiteral("editor.fontSize"), QStringLiteral("Editor"), QStringLiteral("Font size"),
          QString(), T::Int, 12, {}, 8, 48 },
        { QStringLiteral("editor.fontWeight"), QStringLiteral("Editor"), QStringLiteral("Font weight"),
          QStringLiteral("400 = normal, 700 = bold."), T::Int, 400, {}, 300, 900 },
        { QStringLiteral("editor.lineHeight"), QStringLiteral("Editor"), QStringLiteral("Line height"),
          QStringLiteral("Multiplier of the font size."), T::Double, 1.35, {}, 0, 0, 1.0, 3.0 },
        { QStringLiteral("editor.tabSize"), QStringLiteral("Editor"), QStringLiteral("Tab size"),
          QString(), T::Int, 4, {}, 1, 16 },
        { QStringLiteral("editor.insertSpaces"), QStringLiteral("Editor"), QStringLiteral("Insert spaces"),
          QStringLiteral("Use spaces instead of tab characters when indenting."), T::Bool, true, {} },
        { QStringLiteral("editor.wordWrap"), QStringLiteral("Editor"), QStringLiteral("Word wrap"),
          QString(), T::Bool, false, {} },
        { QStringLiteral("editor.showMinimap"), QStringLiteral("Editor"), QStringLiteral("Show minimap"),
          QString(), T::Bool, true, {} },
        { QStringLiteral("editor.showLineNumbers"), QStringLiteral("Editor"), QStringLiteral("Show line numbers"),
          QString(), T::Bool, true, {} },
        { QStringLiteral("editor.showFolding"), QStringLiteral("Editor"), QStringLiteral("Enable code folding"),
          QString(), T::Bool, true, {} },
        { QStringLiteral("editor.highlightActiveLine"), QStringLiteral("Editor"), QStringLiteral("Highlight active line"),
          QString(), T::Bool, true, {} },
        { QStringLiteral("editor.indentGuides"), QStringLiteral("Editor"), QStringLiteral("Indent guides"),
          QString(), T::Bool, true, {} },
        { QStringLiteral("editor.bracketMatching"), QStringLiteral("Editor"), QStringLiteral("Bracket matching"),
          QString(), T::Bool, true, {} },
        { QStringLiteral("editor.autoClosingBrackets"), QStringLiteral("Editor"), QStringLiteral("Auto-closing brackets/quotes"),
          QString(), T::Bool, true, {} },
        { QStringLiteral("editor.autoIndent"), QStringLiteral("Editor"), QStringLiteral("Auto indentation"),
          QString(), T::Bool, true, {} },
        { QStringLiteral("editor.renderWhitespace"), QStringLiteral("Editor"), QStringLiteral("Render whitespace"),
          QString(), T::Enum, QStringLiteral("none"),
          { { QStringLiteral("None"), QStringLiteral("none") }, { QStringLiteral("Boundary"), QStringLiteral("boundary") }, { QStringLiteral("All"), QStringLiteral("all") } } },
        { QStringLiteral("editor.cursorStyle"), QStringLiteral("Editor"), QStringLiteral("Cursor style"),
          QString(), T::Enum, QStringLiteral("line"),
          { { QStringLiteral("Line"), QStringLiteral("line") }, { QStringLiteral("Block"), QStringLiteral("block") }, { QStringLiteral("Underline"), QStringLiteral("underline") } } },
        { QStringLiteral("editor.cursorBlinking"), QStringLiteral("Editor"), QStringLiteral("Cursor blinking"),
          QString(), T::Bool, true, {} },

        // ---- Files ----
        { QStringLiteral("files.autosave"), QStringLiteral("Files"), QStringLiteral("Autosave"),
          QStringLiteral("When modified files are saved automatically."), T::Enum, QStringLiteral("off"),
          { { QStringLiteral("Off"), QStringLiteral("off") },
            { QStringLiteral("After delay"), QStringLiteral("afterDelay") },
            { QStringLiteral("On focus change"), QStringLiteral("onFocusChange") },
            { QStringLiteral("On window blur"), QStringLiteral("onWindowChange") } } },
        { QStringLiteral("files.autosaveDelayMs"), QStringLiteral("Files"), QStringLiteral("Autosave delay (ms)"),
          QString(), T::Int, 1000, {}, 100, 60000 },
        { QStringLiteral("files.trimTrailingWhitespace"), QStringLiteral("Files"), QStringLiteral("Trim trailing whitespace"),
          QStringLiteral("Applied on save."), T::Bool, false, {} },
        { QStringLiteral("files.insertFinalNewline"), QStringLiteral("Files"), QStringLiteral("Insert final newline"),
          QStringLiteral("Applied on save."), T::Bool, false, {} },
        { QStringLiteral("files.defaultEol"), QStringLiteral("Files"), QStringLiteral("Default end of line"),
          QStringLiteral("Used for new files when EOL cannot be detected."), T::Enum, QStringLiteral("auto"),
          { { QStringLiteral("Auto (platform)"), QStringLiteral("auto") }, { QStringLiteral("LF"), QStringLiteral("lf") }, { QStringLiteral("CRLF"), QStringLiteral("crlf") } } },
        { QStringLiteral("files.largeFileWarningMB"), QStringLiteral("Files"), QStringLiteral("Large file warning (MB)"),
          QStringLiteral("Ask before opening files larger than this size. Set 0 to disable."), T::Int, 50, {}, 0, 4096 },

        // ---- Search ----
        { QStringLiteral("search.excludeGlobs"), QStringLiteral("Search"), QStringLiteral("Excluded patterns"),
          QStringLiteral("Glob patterns excluded from workspace search and file index."), T::StringList,
          QStringList{ QStringLiteral("**/.git/**"), QStringLiteral("**/node_modules/**"), QStringLiteral("**/build/**"),
                       QStringLiteral("**/dist/**"), QStringLiteral("**/target/**"), QStringLiteral("**/.codeforge/**") }, {} },
        { QStringLiteral("search.includeHidden"), QStringLiteral("Search"), QStringLiteral("Search hidden files"),
          QString(), T::Bool, false, {} },
        { QStringLiteral("search.maxFileSizeMB"), QStringLiteral("Search"), QStringLiteral("Max file size (MB)"),
          QStringLiteral("Files larger than this are skipped during search."), T::Int, 10, {}, 1, 512 },

        // ---- Terminal ----
        { QStringLiteral("terminal.shell"), QStringLiteral("Terminal"), QStringLiteral("Shell"),
          QStringLiteral("Shell used by the integrated terminal."), T::Enum, QStringLiteral("auto"),
          { { QStringLiteral("Auto detect"), QStringLiteral("auto") }, { QStringLiteral("PowerShell"), QStringLiteral("powershell") },
            { QStringLiteral("PowerShell Core"), QStringLiteral("pwsh") }, { QStringLiteral("CMD"), QStringLiteral("cmd") },
            { QStringLiteral("Git Bash"), QStringLiteral("gitbash") }, { QStringLiteral("Bash"), QStringLiteral("bash") } } },
        { QStringLiteral("terminal.fontSize"), QStringLiteral("Terminal"), QStringLiteral("Font size"),
          QString(), T::Int, 12, {}, 8, 32 },

        // ---- Security ----
        { QStringLiteral("security.clipboardClearSeconds"), QStringLiteral("Security"), QStringLiteral("Clear clipboard after (seconds)"),
          QStringLiteral("Clears the clipboard this many seconds after a copy. 0 disables."), T::Enum, 0,
          { { QStringLiteral("Disabled"), 0 }, { QStringLiteral("15 s"), 15 }, { QStringLiteral("30 s"), 30 }, { QStringLiteral("60 s"), 60 } } },
        { QStringLiteral("security.recoveryEnabled"), QStringLiteral("Security"), QStringLiteral("Crash recovery snapshots"),
          QStringLiteral("Periodically snapshot unsaved documents (encrypted) so they can be restored after a crash."), T::Bool, true, {} },
        { QStringLiteral("security.recoveryIntervalSec"), QStringLiteral("Security"), QStringLiteral("Recovery interval (seconds)"),
          QString(), T::Int, 30, {}, 10, 600 },
        { QStringLiteral("security.encryptRecovery"), QStringLiteral("Security"), QStringLiteral("Encrypt recovery snapshots"),
          QStringLiteral("AES-256-GCM with an OS-protected key. Never store editor content in plaintext."), T::Bool, true, {} },

        // ---- Workspace ----
        { QStringLiteral("workspace.rememberSession"), QStringLiteral("Workspace"), QStringLiteral("Restore session on start"),
          QStringLiteral("Reopen last workspace, tabs and layout."), T::Bool, true, {} },
        { QStringLiteral("run.executable"), QStringLiteral("Workspace"), QStringLiteral("Run executable (relative to workspace)"),
          QStringLiteral("Executable launched by Run; leave empty to auto-detect after build."), T::String, QString(), {} },

        { QStringLiteral("explorer.showHidden"), QStringLiteral("Workspace"), QStringLiteral("Show hidden files in Explorer"),
          QString(), T::Bool, false, {} },
        { QStringLiteral("explorer.previewOnClick"), QStringLiteral("Workspace"), QStringLiteral("Preview files on single click"),
          QStringLiteral("Opening a file from the Explorer replaces the current preview tab."), T::Bool, true, {} },

        // ---- Performance ----
        { QStringLiteral("performance.fileIndexLimit"), QStringLiteral("Performance"), QStringLiteral("File index limit"),
          QStringLiteral("Maximum number of files indexed for Quick Open."), T::Int, 50000, {}, 1000, 500000 },
    };
}

void SettingsManager::load()
{
    m_user = readJson(paths::settingsFile());
    if (!m_workspaceRoot.isEmpty())
        m_workspace = readJson(paths::workspaceSettingsFile(m_workspaceRoot));
}

QString SettingsManager::dataFile() const
{
    return paths::settingsFile();
}

void SettingsManager::setWorkspaceRoot(const QString& root)
{
    m_workspaceRoot = root;
    m_workspace = root.isEmpty() ? QJsonObject() : readJson(paths::workspaceSettingsFile(root));
}

QJsonObject SettingsManager::readJson(const QString& file) const
{
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly)) return QJsonObject();
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    return doc.isObject() ? doc.object() : QJsonObject();
}

void SettingsManager::save(Scope scope)
{
    const QString file = scope == Scope::User ? paths::settingsFile() : paths::workspaceSettingsFile(m_workspaceRoot);
    const QJsonObject& obj = scope == Scope::User ? m_user : m_workspace;

    cf::fs::ensureParentDir(file);
    QSaveFile f(file);
    if (!f.open(QIODevice::WriteOnly)) {
        CF_LOG_ERROR(QStringLiteral("Settings: cannot write %1").arg(file));
        return;
    }
    f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    f.commit();
}

QVariant SettingsManager::lookup(const QJsonObject& obj, const QString& key, bool* found) const
{
    if (found) *found = false;
    const QStringList parts = key.split(QLatin1Char('.'));
    QJsonObject cur = obj;
    for (int i = 0; i < parts.size() - 1; ++i) {
        if (!cur.contains(parts.at(i))) return QVariant();
        cur = cur.value(parts.at(i)).toObject();
    }
    if (!cur.contains(parts.last())) return QVariant();
    if (found) *found = true;
    return cur.value(parts.last()).toVariant();
}

QVariant SettingsManager::get(const QString& key) const
{
    bool found = false;
    QVariant v = lookup(m_workspace, key, &found);
    if (found) return v;
    v = lookup(m_user, key, &found);
    if (found) return v;
    const SettingDef* d = def(key);
    return d ? d->defaultValue : QVariant();
}

const SettingDef* SettingsManager::def(const QString& key) const
{
    for (const SettingDef& d : m_defs)
        if (d.key == key) return &d;
    return nullptr;
}

static QJsonObject insertKey(QJsonObject obj, const QString& key, const QJsonValue& value)
{
    const QStringList parts = key.split(QLatin1Char('.'));
    if (parts.size() == 1) { obj.insert(parts.first(), value); return obj; }
    QJsonObject inner = obj.value(parts.first()).toObject();
    inner.insert(parts.last(), value);
    obj.insert(parts.first(), inner);
    return obj;
}

void SettingsManager::set(const QString& key, const QVariant& value, Scope scope)
{
    const QVariant old = get(key);
    if (scope == Scope::User) {
        m_user = insertKey(m_user, key, QJsonValue::fromVariant(value));
        save(Scope::User);
    } else {
        m_workspace = insertKey(m_workspace, key, QJsonValue::fromVariant(value));
        save(Scope::Workspace);
    }
    if (old != value) emit changed(key, value);
}

void SettingsManager::resetAll()
{
    m_user = QJsonObject();
    m_workspace = QJsonObject();
    save(Scope::User);
    if (!m_workspaceRoot.isEmpty()) save(Scope::Workspace);
    for (const SettingDef& d : m_defs) emit changed(d.key, d.defaultValue);
}

bool SettingsManager::exportTo(const QString& file, QString* error) const
{
    QSaveFile f(file);
    if (!f.open(QIODevice::WriteOnly)) {
        if (error) *error = f.errorString();
        return false;
    }
    f.write(QJsonDocument(m_user).toJson(QJsonDocument::Indented));
    if (!f.commit()) {
        if (error) *error = f.errorString();
        return false;
    }
    return true;
}

bool SettingsManager::importFrom(const QString& file, QString* error)
{
    m_user = readJson(file);
    if (m_user.isEmpty()) {
        if (error) *error = QStringLiteral("Not a valid settings file.");
        return false;
    }
    save(Scope::User);
    for (const SettingDef& d : m_defs) emit changed(d.key, get(d.key));
    return true;
}

}  // namespace cf
