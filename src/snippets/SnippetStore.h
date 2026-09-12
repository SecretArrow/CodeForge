#pragma once
// SnippetStore: code snippets with per-language scoping.
//   - built-in snippets compiled into the binary (works offline out of the box)
//   - user snippets in dataRoot()/snippets.json
//   - workspace snippets in <workspace>/.codeforge/snippets.json
// Bodies may contain a "$0" marker that receives the caret after expansion.
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

class CodeEditor;

namespace cf {

class CompletionItem;

struct Snippet {
    QString trigger;
    QString body;
    QString description;
    QStringList languages;   // empty = all languages
};

class SnippetStore : public QObject {
    Q_OBJECT
public:
    static SnippetStore& instance();

    void load();                                  // user + workspace files
    void setWorkspaceRoot(const QString& root);   // "" = none

    // snippets applicable to a language id ("" = plain text -> all-language only)
    QVector<Snippet> forLanguage(const QString& languageId) const;

    // completion items for the popup (isSnippet=true, body carried along)
    QVector<CompletionItem> completionItems(const QString& languageId) const;

    // Expand `trigger` at the editor's cursor if it matches. Returns true on
    // expansion (the document changed).
    bool expandIfMatch(CodeEditor* editor, const QString& languageId, const QString& trigger) const;

    // Pure expansion helper (unit-tested): replaces the trigger text before
    // the cursor with the body, places the caret at "$0".
    static bool expandAtCursor(CodeEditor* editor, const QString& body);

    // user store manipulation (SnippetsDialog)
    QVector<Snippet> userSnippets() const;
    void setUserSnippets(const QVector<Snippet>& snippets);   // persists + reloads
    QString userSnippetsPath() const;

signals:
    void changed();

private:
    SnippetStore() = default;
    static QVector<Snippet> builtinSnippets();
    static QVector<Snippet> loadFile(const QString& path);
    bool saveUserFile(const QVector<Snippet>& snippets) const;

    QVector<Snippet> m_user;
    QVector<Snippet> m_workspace;
    QString m_workspaceRoot;
    bool m_loaded = false;
};

}  // namespace cf
