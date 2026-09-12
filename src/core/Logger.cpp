#include "core/Logger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTextStream>

namespace cf {

static constexpr int kMaxLogBytes = 2 * 1024 * 1024;
static constexpr int kMaxLogFiles = 5;

Logger& Logger::instance()
{
    static Logger logger;
    return logger;
}

void Logger::init(const QString& dir)
{
    QMutexLocker lock(&m_mutex);
    m_dir = dir + QStringLiteral("/logs");
    QDir().mkpath(m_dir);
    const QString path = m_dir + QStringLiteral("/codeforge.log");
    if (m_file) { m_file->close(); delete m_file; m_file = nullptr; }
    m_file = new QFile(path);
    if (m_file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        m_currentSize = m_file->size();
        m_ready = true;
    }
}

void Logger::shutdown()
{
    QMutexLocker lock(&m_mutex);
    if (m_file) { m_file->close(); delete m_file; m_file = nullptr; }
    m_ready = false;
}

QString Logger::sanitize(const QString& msg)
{
    QString out = msg;
    out.replace(QLatin1Char('\n'), QStringLiteral("\\n"));
    out.replace(QLatin1Char('\r'), QStringLiteral("\\r"));
    out.replace(QLatin1Char('\t'), QStringLiteral("\\t"));
    if (out.size() > 600) out = out.left(600) + QStringLiteral("...[truncated]");
    return out;
}

void Logger::rotateIfNeeded()
{
    // Assumes m_mutex held.
    if (!m_file || m_currentSize < kMaxLogBytes) return;
    m_file->close();
    for (int i = kMaxLogFiles - 1; i >= 1; --i) {
        const QString from = m_dir + QStringLiteral("/codeforge.log.%1").arg(i);
        const QString to   = m_dir + QStringLiteral("/codeforge.log.%1").arg(i + 1);
        QFile::remove(to);
        QFile::rename(from, to);
    }
    QFile::remove(m_dir + QStringLiteral("/codeforge.log.1"));
    QFile::rename(m_dir + QStringLiteral("/codeforge.log"), m_dir + QStringLiteral("/codeforge.log.1"));
    m_file->close();
    delete m_file;
    m_file = new QFile(m_dir + QStringLiteral("/codeforge.log"));
    m_file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    m_currentSize = 0;
}

QString Logger::levelTag(Level level) const
{
    switch (level) {
    case Level::Debug:   return QStringLiteral("DEBUG");
    case Level::Info:    return QStringLiteral("INFO");
    case Level::Warning: return QStringLiteral("WARN");
    case Level::Error:   return QStringLiteral("ERROR");
    }
    return QStringLiteral("INFO");
}

void Logger::write(Level level, const QString& msg)
{
    QMutexLocker lock(&m_mutex);
    if (!m_ready) return;
    rotateIfNeeded();
    const QString line = QStringLiteral("[%1] [%2] %3\n")
                             .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz")),
                                  levelTag(level), sanitize(msg));
    m_file->write(line.toUtf8());
    m_currentSize += line.toUtf8().size();
    m_file->flush();
}

}  // namespace cf
