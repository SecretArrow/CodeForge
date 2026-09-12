#include "themes/Theme.h"

#include <QJsonArray>

namespace cf {

bool Theme::parse(const QByteArray& json)
{
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return false;
    const QJsonObject root = doc.object();

    m_id = root.value(QStringLiteral("id")).toString();
    m_name = root.value(QStringLiteral("name")).toString();
    m_dark = root.value(QStringLiteral("type")).toString() != QStringLiteral("light");
    if (m_id.isEmpty()) return false;

    m_colors.clear();
    const QJsonObject colors = root.value(QStringLiteral("colors")).toObject();
    for (auto it = colors.begin(); it != colors.end(); ++it)
        m_colors.insert(it.key(), it.value().toString());
    return !m_colors.isEmpty();
}

QColor Theme::color(const QString& token) const
{
    const auto it = m_colors.constFind(token);
    if (it != m_colors.constEnd()) return QColor(*it);

    // Sensible fallbacks derived from base colors.
    if (token == QStringLiteral("editor.background")) return m_dark ? QColor(0x1e, 0x1e, 0x1e) : QColor(0xff, 0xff, 0xff);
    if (token == QStringLiteral("editor.foreground")) return m_dark ? QColor(0xd4, 0xd4, 0xd4) : QColor(0x3b, 0x3b, 0x3b);
    if (token == QStringLiteral("ui.background")) return editorBackground();
    if (token == QStringLiteral("ui.foreground")) return editorForeground();
    if (token == QStringLiteral("ui.border")) return m_dark ? QColor(0x3c, 0x3c, 0x3c) : QColor(0xdc, 0xdc, 0xdc);
    if (token == QStringLiteral("ui.accent")) return m_dark ? QColor(0x0e, 0x63, 0x9c) : QColor(0x00, 0x5f, 0xb8);
    if (token == QStringLiteral("editor.indentGuide")) return m_dark ? QColor(0x40, 0x40, 0x40) : QColor(0xd3, 0xd3, 0xd3);
    if (token.startsWith(QStringLiteral("syntax."))) return editorForeground();
    if (m_dark) return QColor(0x25, 0x25, 0x26);
    return QColor(0xf8, 0xf8, 0xf8);
}

QColor Theme::color(const QString& token, const QColor& fallback) const
{
    const auto it = m_colors.constFind(token);
    if (it != m_colors.constEnd()) return QColor(*it);
    return fallback;
}

QStringList Theme::knownTokens()
{
    return {
        QStringLiteral("ui.background"), QStringLiteral("ui.foreground"),
        QStringLiteral("ui.sidebar"), QStringLiteral("ui.activityBar"),
        QStringLiteral("ui.statusBar"), QStringLiteral("ui.statusBarText"),
        QStringLiteral("ui.tabBar"), QStringLiteral("ui.tab.active"), QStringLiteral("ui.tab.inactive"),
        QStringLiteral("ui.border"), QStringLiteral("ui.accent"), QStringLiteral("ui.accentText"),
        QStringLiteral("ui.input"), QStringLiteral("ui.inputBorder"),
        QStringLiteral("ui.list.hover"), QStringLiteral("ui.list.selected"),
        QStringLiteral("ui.button"), QStringLiteral("ui.buttonText"),
        QStringLiteral("ui.disabledText"), QStringLiteral("ui.link"),
        QStringLiteral("ui.error"), QStringLiteral("ui.warning"), QStringLiteral("ui.success"),
        QStringLiteral("ui.panel"), QStringLiteral("ui.tooltip"),
        QStringLiteral("editor.background"), QStringLiteral("editor.foreground"),
        QStringLiteral("editor.selection"), QStringLiteral("editor.inactiveSelection"),
        QStringLiteral("editor.cursor"), QStringLiteral("editor.lineNumber"),
        QStringLiteral("editor.lineNumberActive"), QStringLiteral("editor.activeLine"),
        QStringLiteral("editor.findMatch"), QStringLiteral("editor.currentFindMatch"),
        QStringLiteral("editor.indentGuide"), QStringLiteral("editor.bracketMatch"),
        QStringLiteral("syntax.keyword"), QStringLiteral("syntax.control"),
        QStringLiteral("syntax.string"), QStringLiteral("syntax.comment"),
        QStringLiteral("syntax.number"), QStringLiteral("syntax.function"),
        QStringLiteral("syntax.type"), QStringLiteral("syntax.variable"),
        QStringLiteral("syntax.constant"), QStringLiteral("syntax.operator"),
        QStringLiteral("syntax.preprocessor"), QStringLiteral("syntax.tag"),
        QStringLiteral("syntax.attribute"), QStringLiteral("syntax.property"),
        QStringLiteral("syntax.heading"), QStringLiteral("syntax.link")
    };
}

}  // namespace cf
