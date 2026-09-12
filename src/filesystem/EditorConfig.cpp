#include "filesystem/EditorConfig.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

namespace cf {

namespace {

QString globToRegex(QString pattern)
{
    QString out;
    out.reserve(pattern.size() + 8);
    for (int i = 0; i < pattern.size(); ++i) {
        const QChar c = pattern.at(i);
        switch (c.unicode()) {
        case u'*':
            if (i + 1 < pattern.size() && pattern.at(i + 1) == u'*') {
                out += QStringLiteral(".*");
                ++i;   // consume second '*'
            } else {
                out += QStringLiteral("[^/]*");
            }
            break;
        case u'?': out += QStringLiteral("[^/]"); break;
        case u'{': {
            // {a,b} -> (a|b)   (no nesting for simplicity)
            const int close = pattern.indexOf(QLatin1Char('}'), i);
            if (close < 0) { out += QRegularExpression::escape(c); break; }
            const QString inner = pattern.mid(i + 1, close - i - 1);
            QStringList parts;
            for (const QString& p : inner.split(QLatin1Char(',')))
                parts << QRegularExpression::escape(p.trimmed());
            out += QStringLiteral("(") + parts.join(QStringLiteral("|")) + QStringLiteral(")");
            i = close;
            break;
        }
        case u'[': out += QRegularExpression::escape(c); break;
        case u'.': case u'\\': case u'(': case u')':
        case u'+': case u'^': case u'$': case u'|':
            out += QRegularExpression::escape(c);
            break;
        default: out += c; break;
        }
    }
    return out;
}

struct Section {
    QString pattern;
    QStringList lines;
};

// Parse one .editorconfig file: returns global lines + sections in order.
void parseFile(const QString& content, QStringList* globalLines, QList<Section>* sections)
{
    Section current;
    bool inSection = false;
    const QStringList raw = content.split(QLatin1Char('\n'));
    for (QString line : raw) {
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')) || line.startsWith(QLatin1Char(';'))) continue;
        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
            if (inSection) sections->append(current);
            current = Section();
            current.pattern = line.mid(1, line.size() - 2).trimmed();
            inSection = true;
            continue;
        }
        if (inSection) current.lines.append(line);
        else globalLines->append(line);
    }
    if (inSection) sections->append(current);
}

void applyKeyValue(EditorConfigProps& props, const QString& key, const QString& rawValue)
{
    const QString value = rawValue.trimmed();
    if (key == QLatin1String("indent_style")) {
        props.indentStyle = value.toLower();
    } else if (key == QLatin1String("indent_size")) {
        if (value.compare(QLatin1String("tab"), Qt::CaseInsensitive) == 0) {
            // "tab" means: use tab_width (resolved later; default 4).
            props.indentSize = -2;
        } else {
            bool ok = false;
            const int n = value.toInt(&ok);
            if (ok) props.indentSize = n;
        }
    } else if (key == QLatin1String("tab_width")) {
        bool ok = false;
        const int n = value.toInt(&ok);
        if (ok) props.tabWidth = n;
    } else if (key == QLatin1String("end_of_line")) {
        props.endOfLine = value.toLower();
    } else if (key == QLatin1String("charset")) {
        props.charset = value.toLower();
    } else if (key == QLatin1String("trim_trailing_whitespace")) {
        props.trimTrailing = value.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0;
        props.trimTrailingSet = true;
    } else if (key == QLatin1String("insert_final_newline")) {
        props.finalNewline = value.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0;
        props.finalNewlineSet = true;
    }
}

}  // namespace

bool EditorConfig::sectionMatches(const QString& pattern, const QString& relativePath)
{
    const QString path = QDir::fromNativeSeparators(relativePath);
    const QString base = QFileInfo(path).fileName();

    // Patterns without a slash match the basename only.
    const QString target = pattern.contains(QLatin1Char('/')) ? path : base;
    QRegularExpression re(QStringLiteral("^") + globToRegex(pattern) + QStringLiteral("$"));
    if (!re.isValid()) return false;
    return re.match(target).hasMatch();
}

EditorConfigProps EditorConfig::parseProps(const QStringList& lines)
{
    EditorConfigProps props;
    for (const QString& line : lines) {
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0) continue;
        applyKeyValue(props, line.left(eq).trimmed().toLower(), line.mid(eq + 1));
    }
    return props;
}

EditorConfigProps EditorConfig::resolve(const QString& filePath, const QString& workspaceRoot)
{
    QFileInfo info(filePath);
    const QString abs = info.absoluteFilePath();
    QDir dir = info.isAbsolute() ? info.dir() : QDir(QFileInfo(QDir::currentPath(), filePath).absolutePath());

    // Collect .editorconfig files from the file's dir upward.
    struct Found { QDir dir; QString content; bool isRoot; };
    QList<Found> found;   // innermost first
    bool stop = false;
    while (!stop) {
        const QString cfg = dir.filePath(QStringLiteral(".editorconfig"));
        if (QFileInfo::exists(cfg)) {
            QFile f(cfg);
            const QString content = f.open(QIODevice::ReadOnly | QIODevice::Text) ? QString::fromUtf8(f.readAll()) : QString();
            // root = true in the global section stops the walk.
            bool isRoot = false;
            const QStringList raw = content.split(QLatin1Char('\n'));
            for (const QString& lineRaw : raw) {
                const QString line = lineRaw.trimmed();
                if (line.startsWith(QLatin1Char('['))) break;   // global section ended
                if (line.startsWith(QLatin1String("root"))) {
                    const int eq = line.indexOf(QLatin1Char('='));
                    if (eq > 0 && line.mid(eq + 1).trimmed().compare(QLatin1String("true"), Qt::CaseInsensitive) == 0)
                        isRoot = true;
                }
            }
            found.append({dir, content, isRoot});
            if (isRoot) break;
        }
        // Stop at workspace root if provided.
        if (!workspaceRoot.isEmpty() && dir.absolutePath() == QDir(workspaceRoot).absolutePath()) break;
        if (!dir.cdUp()) break;
        // Safety: don't walk above the filesystem root.
        if (dir.isRoot()) stop = true;
    }

    if (found.isEmpty()) return EditorConfigProps();

    // Apply farthest-first so the closest file wins (later overrides).
    EditorConfigProps props;
    for (int i = found.size() - 1; i >= 0; --i) {
        QStringList globalLines;
        QList<Section> sections;
        parseFile(found.at(i).content, &globalLines, &sections);

        QString rel = QDir(found.at(i).dir).relativeFilePath(abs);
        rel = QDir::fromNativeSeparators(rel);

        // Global section props (rare, applies to everything) then sections.
        EditorConfigProps g = parseProps(globalLines);
        if (g.valid || !globalLines.isEmpty()) {
            g.valid = true;
            const bool tt = props.trimTrailingSet, fn = props.finalNewlineSet;
            const int is = props.indentSize, tw = props.tabWidth;
            const QString st = props.indentStyle, eol = props.endOfLine, cs = props.charset;
            props = g;
            props.trimTrailingSet = props.trimTrailingSet || tt;
            props.finalNewlineSet = props.finalNewlineSet || fn;
            if (props.indentSize == -1) props.indentSize = is;
            if (props.indentSize == -2) props.indentSize = -2;   // "tab" marker survives
            if (props.tabWidth == -1) props.tabWidth = tw;
            if (props.indentStyle.isEmpty()) props.indentStyle = st;
            if (props.endOfLine.isEmpty()) props.endOfLine = eol;
            if (props.charset.isEmpty()) props.charset = cs;
        }
        for (const Section& s : sections) {
            if (!sectionMatches(s.pattern, rel)) continue;
            const EditorConfigProps p = parseProps(s.lines);
            props.valid = true;
            if (!p.indentStyle.isEmpty()) props.indentStyle = p.indentStyle;
            if (p.indentSize != -1) props.indentSize = p.indentSize;
            if (p.tabWidth != -1) props.tabWidth = p.tabWidth;
            if (!p.endOfLine.isEmpty()) props.endOfLine = p.endOfLine;
            if (!p.charset.isEmpty()) props.charset = p.charset;
            if (p.trimTrailingSet) { props.trimTrailing = p.trimTrailing; props.trimTrailingSet = true; }
            if (p.finalNewlineSet) { props.finalNewline = p.finalNewline; props.finalNewlineSet = true; }
        }
    }

    // Resolve indent_size = "tab" using tab_width (default 4).
    if (props.indentSize == -2) props.indentSize = props.tabWidth > 0 ? props.tabWidth : 4;
    if (props.indentSize <= 0) props.indentSize = -1;
    return props;
}

}  // namespace cf
