#pragma once
// Common types and small utilities shared across CodeForge modules.
#include <QDir>
#include <QString>
#include <QStringList>
#include <QVariant>

namespace cf {

// ---------- Line endings ----------
enum class LineEndings { Lf, Crlf, Cr };

inline QString lineEndingsToString(LineEndings e)
{
    switch (e) {
    case LineEndings::Crlf: return QStringLiteral("CRLF");
    case LineEndings::Cr:   return QStringLiteral("CR");
    case LineEndings::Lf:   return QStringLiteral("LF");
    }
    return QStringLiteral("LF");
}

inline LineEndings lineEndingsFromString(const QString& s)
{
    if (s.compare(QStringLiteral("CRLF"), Qt::CaseInsensitive) == 0) return LineEndings::Crlf;
    if (s.compare(QStringLiteral("CR"), Qt::CaseInsensitive) == 0)  return LineEndings::Cr;
    return LineEndings::Lf;
}

inline QString eolCharacters(LineEndings e)
{
    switch (e) {
    case LineEndings::Crlf: return QStringLiteral("\r\n");
    case LineEndings::Cr:   return QStringLiteral("\r");
    case LineEndings::Lf:   return QStringLiteral("\n");
    }
    return QStringLiteral("\n");
}

// ---------- Small helpers ----------
inline QString formatBytes(qint64 bytes)
{
    if (bytes < 1024) return QStringLiteral("%1 B").arg(bytes);
    double v = double(bytes);
    const QStringList units = { QStringLiteral("KB"), QStringLiteral("MB"), QStringLiteral("GB"), QStringLiteral("TB") };
    int u = -1;
    while (v >= 1024.0 && u < units.size() - 1) { v /= 1024.0; ++u; }
    return QString::number(v, 'f', v < 10 ? 1 : 0) + QLatin1Char(' ') + units.value(u);
}

inline QString fileExtensionOf(const QString& path)
{
    const int slash = path.lastIndexOf(QLatin1Char('/'));
    const QString name = slash >= 0 ? path.mid(slash + 1) : path;
    const int dot = name.lastIndexOf(QLatin1Char('.'));
    if (dot <= 0) return QString();  // dotfiles like ".gitignore" have no extension
    return name.mid(dot + 1).toLower();
}

inline QString baseNameOf(const QString& path)
{
    const int slash = path.lastIndexOf(QLatin1Char('/'));
    QString name = slash >= 0 ? path.mid(slash + 1) : path;
    const int dot = name.lastIndexOf(QLatin1Char('.'));
    if (dot > 0) name.truncate(dot);
    return name;
}

inline QString toNativePath(const QString& p) { return QDir::fromNativeSeparators(p); }

inline bool isProbablyBinary(const QByteArray& head)
{
    const int n = qMin<int>(head.size(), 8192);
    for (int i = 0; i < n; ++i)
        if (head.at(i) == '\0') return true;
    return false;
}

}  // namespace cf
