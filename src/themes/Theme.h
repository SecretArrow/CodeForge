#pragma once
// Theme data model: JSON-based color tokens for UI + editor + syntax.
// Users can drop their own theme JSON into the themes folder.
#include <QColor>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QString>

namespace cf {

class Theme {
public:
    bool parse(const QByteArray& json);

    QString id() const { return m_id; }
    QString name() const { return m_name; }
    bool isDark() const { return m_dark; }

    // Returns token color; falls back to sane defaults derived from editor colors.
    QColor color(const QString& token) const;
    // Returns token color; falls back to the caller-provided color.
    QColor color(const QString& token, const QColor& fallback) const;
    bool hasColor(const QString& token) const { return m_colors.contains(token); }

    // Editor helpers
    QColor editorBackground() const { return color(QStringLiteral("editor.background")); }
    QColor editorForeground() const { return color(QStringLiteral("editor.foreground")); }
    QColor syntaxColor(const QString& token) const { return color(QStringLiteral("syntax.") + token); }

    static QStringList knownTokens();

private:
    QString m_id;
    QString m_name;
    bool m_dark = true;
    QMap<QString, QString> m_colors;   // token -> "#rrggbb"
};

}  // namespace cf
