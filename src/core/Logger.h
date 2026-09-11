#pragma once
// Rotating application logger.
// SECURITY: never pass file contents, credentials, tokens, clipboard text or
// any sensitive buffer content to the log. Only paths, counts and event names.
#include <QMutex>
#include <QObject>
#include <QString>

class QFile;

namespace cf {

class Logger : public QObject {
    Q_OBJECT
public:
    enum class Level { Debug, Info, Warning, Error };

    static Logger& instance();

    // dir: base data directory; creates "logs" subfolder.
    void init(const QString& dir);
    void shutdown();

    void debug(const QString& msg)   { write(Level::Debug, msg); }
    void info(const QString& msg)    { write(Level::Info, msg); }
    void warning(const QString& msg) { write(Level::Warning, msg); }
    void error(const QString& msg)   { write(Level::Error, msg); }

    // Strip newlines / control chars and cap length, for safe single-line logging.
    static QString sanitize(const QString& msg);

private:
    Logger() = default;
    void write(Level level, const QString& msg);
    void rotateIfNeeded();
    QString levelTag(Level level) const;

    QMutex m_mutex;
    QFile* m_file = nullptr;
    QString m_dir;
    qint64 m_currentSize = 0;
    bool m_ready = false;
};

}  // namespace cf

#define CF_LOG_DEBUG(msg)   ::cf::Logger::instance().debug(msg)
#define CF_LOG_INFO(msg)    ::cf::Logger::instance().info(msg)
#define CF_LOG_WARNING(msg) ::cf::Logger::instance().warning(msg)
#define CF_LOG_ERROR(msg)   ::cf::Logger::instance().error(msg)
