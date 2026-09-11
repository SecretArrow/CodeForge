#include "app/Application.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTimer>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#endif

#include "core/AppPaths.h"
#include "core/Logger.h"
#include "core/DocumentManager.h"
#include "core/Logger.h"
#include "project/SessionManager.h"
#include "settings/SettingsManager.h"
#include "themes/ThemeManager.h"
#include "ui/MainWindow.h"

namespace cf {

static void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    Q_UNUSED(context);
    switch (type) {
    case QtDebugMsg: Logger::instance().debug(msg); break;
    case QtInfoMsg: Logger::instance().info(msg); break;
    case QtWarningMsg: Logger::instance().warning(msg); break;
    case QtCriticalMsg:
    case QtFatalMsg: Logger::instance().error(msg); break;
    }
}

#ifdef Q_OS_WIN
// Mini crash marker (no content, ever).
static LONG WINAPI crashHandler(EXCEPTION_POINTERS*)
{
    Logger::instance().error(QStringLiteral("Unhandled exception (crash marker)"));
    return EXCEPTION_EXECUTE_HANDLER;
}

static void enableDarkTitleBar(HWND hwnd, bool dark)
{
    BOOL value = dark ? TRUE : FALSE;
    // Attribute 20 on newer builds, 19 on older ones.
    if (DwmSetWindowAttribute(hwnd, 20, &value, sizeof(value)) != S_OK)
        DwmSetWindowAttribute(hwnd, 19, &value, sizeof(value));
}
#endif

bool Application::anotherInstanceRunning(const QStringList& args)
{
    const QString serverName = QStringLiteral("codeforge-instance-%1")
                                   .arg(QCoreApplication::applicationFilePath().section(QLatin1Char('/'), -1));

    QLocalSocket socket;
    socket.connectToServer(serverName);
    if (socket.waitForConnected(300)) {
        socket.write(args.join(u'\n').toUtf8());
        socket.flush();
        socket.waitForBytesWritten(300);
        return true;
    }
    return false;
}

int Application::run(int argc, char** argv)
{
    qInstallMessageHandler(messageHandler);

    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("CodeForgeProject"));
    QApplication::setApplicationName(QStringLiteral("CodeForge"));
    QApplication::setApplicationVersion(QStringLiteral(CF_APP_VERSION));

#ifdef Q_OS_WIN
    SetUnhandledExceptionFilter(crashHandler);
#endif

    Logger::instance().init(paths::dataRoot());
    CF_LOG_INFO(QStringLiteral("CodeForge %1 starting (portable=%2)")
                    .arg(QStringLiteral(CF_APP_VERSION))
                    .arg(paths::isPortable() ? QStringLiteral("yes") : QStringLiteral("no")));

    // Single instance: forward args and exit.
    QStringList args = QApplication::arguments();
    args.removeFirst();
    if (anotherInstanceRunning(args)) {
        CF_LOG_INFO(QStringLiteral("Instance already running; args forwarded"));
        return 0;
    }

    // Accept forwarded args in later launches.
    const QString serverName = QStringLiteral("codeforge-instance-%1")
                                   .arg(QCoreApplication::applicationFilePath().section(QLatin1Char('/'), -1));
    QLocalServer::removeServer(serverName);
    QLocalServer* server = new QLocalServer(&app);
    server->listen(serverName);
    QObject::connect(server, &QLocalServer::newConnection, &app, [server, &app]() {
        QLocalSocket* conn = server->nextPendingConnection();
        if (!conn) return;
        QObject::connect(conn, &QLocalSocket::readyRead, &app, [conn]() {
            const QStringList paths = QString::fromUtf8(conn->readAll()).split(u'\n', Qt::SkipEmptyParts);
            for (QWidget* w : QApplication::topLevelWidgets()) {
                if (auto* mw = qobject_cast<MainWindow*>(w)) {
                    mw->openCommandLineTargets(paths);
                    mw->raise();
                    mw->activateWindow();
                }
            }
            conn->deleteLater();
        });
    });

    // Boot order: settings -> themes -> window.
    SettingsManager::instance().load();
    MainWindow* window = new MainWindow();
    window->show();

#ifdef Q_OS_WIN
    enableDarkTitleBar(reinterpret_cast<HWND>(window->winId()),
                       ThemeManager::instance().currentTheme().isDark());
#endif

    // Session restore or command-line targets.
    SessionManager session;
    if (!args.isEmpty()) {
        window->openCommandLineTargets(args);
    } else {
        window->showRecoveryDialog();   // crash recovery comes first
        if (SettingsManager::instance().getBool(QStringLiteral("workspace.rememberSession")))
            window->restoreSession(session.loadSession());
    }

    const int code = QApplication::exec();
    Logger::instance().shutdown();
    return code;
}

}  // namespace cf
