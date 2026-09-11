#pragma once
// Small service-locator passed to extensions; the app wires it at startup.
// (Core managers are singletons; this struct exists so plugins get a single
// stable entry point and so dependency direction stays explicit.)
#include <QString>

namespace cf {

class SettingsManager;
class ThemeManager;
class Workspace;
class DocumentManager;
class CommandRegistry;
class GitClient;
class BuildManager;

struct AppContext {
    SettingsManager* settings = nullptr;
    ThemeManager* themes = nullptr;
    Workspace* workspace = nullptr;
    DocumentManager* documents = nullptr;
    CommandRegistry* commands = nullptr;
    GitClient* git = nullptr;
    BuildManager* build = nullptr;
    QString version;
};

}  // namespace cf
