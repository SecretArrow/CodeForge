#pragma once
// Application bootstrap: single-instance guard, crash log hook, Windows dark
// title bar, portable detection, session restore.
#include <QStringList>

namespace cf {

class Application {
public:
    // Runs the app; returns process exit code.
    static int run(int argc, char** argv);

    static bool anotherInstanceRunning(const QStringList& args);
};

}  // namespace cf
