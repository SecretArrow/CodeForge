#include "extensions/ExtensionHost.h"

#include <QCoreApplication>
#include <QDir>
#include <QPluginLoader>

#include "core/AppPaths.h"
#include "core/Logger.h"
#include "core/AppContext.h"

namespace cf {

ExtensionHost::ExtensionHost(QObject* parent) : QObject(parent) {}

QString ExtensionHost::extensionsFolder() const
{
    return paths::extensionsDir();
}

void ExtensionHost::loadAll(const AppContext& context)
{
    m_entries.clear();
    m_plugins.clear();

    // Built-in data packs: themes embedded as resources are the first
    // "extension surfaces" — reported so the Extensions view isn't empty.
    Entry builtinThemes;
    builtinThemes.id = QStringLiteral("builtin.themes");
    builtinThemes.name = QStringLiteral("Built-in Themes");
    builtinThemes.version = QStringLiteral("1.0.0");
    builtinThemes.description = QStringLiteral("Dark+, Light+, Dracula-like, Monokai-like, High Contrast themes.");
    builtinThemes.loaded = true;
    m_entries.append(builtinThemes);

    Entry builtinLanguages;
    builtinLanguages.id = QStringLiteral("builtin.languages");
    builtinLanguages.name = QStringLiteral("Language Pack");
    builtinLanguages.version = QStringLiteral("1.0.0");
    builtinLanguages.description = QStringLiteral("Syntax highlighting for 20+ languages (C/C++, C#, Java, JS/TS, Python, Rust, Go, PHP, HTML, CSS, JSON, XML, YAML, Markdown, SQL, Shell, PowerShell, CMake, INI, TOML).");
    builtinLanguages.loaded = true;
    m_entries.append(builtinLanguages);

    scanFolder(paths::extensionsDir(), context);
    emit loadFinished(int(m_plugins.size()), paths::extensionsDir());
}

void ExtensionHost::scanFolder(const QString& folder, const AppContext& context)
{
    QDir dir(folder);
    if (!dir.exists()) return;

    for (const QFileInfo& fi : dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
        // Plugin candidates: *.dll / *.so directly, or inside a subfolder.
        const QString name = fi.fileName();
        if (!(name.endsWith(QStringLiteral(".dll"), Qt::CaseInsensitive) ||
              name.endsWith(QStringLiteral(".so"), Qt::CaseInsensitive))) {
            if (fi.isDir())
                scanFolder(fi.absoluteFilePath(), context);
            continue;
        }

        Entry entry;
        entry.sourceFile = fi.absoluteFilePath();

        QPluginLoader loader(fi.absoluteFilePath());
        if (loader.load()) {
            QObject* root = loader.instance();
            if (auto* plugin = qobject_cast<IExtension*>(root)) {
                if (plugin->initialize(const_cast<AppContext&>(context))) {
                    m_plugins.append(plugin);
                    entry.id = plugin->id();
                    entry.name = plugin->name();
                    entry.version = plugin->version();
                    entry.description = plugin->description();
                    entry.loaded = true;
                    CF_LOG_INFO(QStringLiteral("Extensions: loaded %1 (%2)").arg(entry.name, entry.id));
                } else {
                    entry.error = QStringLiteral("initialize() returned false");
                    loader.unload();
                }
            } else {
                entry.error = QStringLiteral("Not a CodeForge extension (IExtension interface missing)");
                loader.unload();
            }
        } else {
            entry.error = loader.errorString();
            CF_LOG_WARNING(QStringLiteral("Extensions: failed to load %1: %2").arg(name, entry.error));
        }
        if (entry.id.isEmpty()) {
            entry.id = name;
            entry.name = name;
        }
        m_entries.append(entry);
    }
}

}  // namespace cf
