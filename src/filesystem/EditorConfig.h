#pragma once
// .editorconfig support: parses .editorconfig files walking up from the
// file's directory (stops after a root=true file) and resolves properties
// for a given file via glob section matching. Pure logic, unit-testable.
#include <QString>

namespace cf {

struct EditorConfigProps {
    bool valid = false;            // any matching section found
    QString indentStyle;           // "space" | "tab" (empty = unset)
    int indentSize = -1;           // -1 = unset ("tab" resolves to tabWidth)
    int tabWidth = -1;             // -1 = unset
    QString charset;               // informational
    QString endOfLine;             // "lf" | "crlf" | "cr" (empty = unset)
    bool trimTrailing = false;
    bool trimTrailingSet = false;
    bool finalNewline = false;
    bool finalNewlineSet = false;
};

class EditorConfig {
public:
    // Walks up from filePath's directory collecting .editorconfig files
    // (closest wins; climbing stops after a root=true file). workspaceRoot
    // is optional and only limits how far up the walk goes (empty = to the
    // filesystem root).
    static EditorConfigProps resolve(const QString& filePath, const QString& workspaceRoot);

    // Glob section matching per EditorConfig spec subset:
    // * (no slash), ** (any), ? (single char), {a,b} alternation.
    static bool sectionMatches(const QString& pattern, const QString& relativePath);

    // Parse "key = value" lines of one section into props.
    static EditorConfigProps parseProps(const QStringList& lines);
};

}  // namespace cf
