#include "snippets/SnippetStore.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextCursor>
#include <QFileInfo>
#include <QTextDocument>

#include "core/AppPaths.h"
#include "core/FileUtils.h"
#include "core/Logger.h"

// The completion item type lives in the autocomplete module.
#include "autocomplete/CompletionEngine.h"
#include "editor/CodeEditor.h"

namespace cf {

SnippetStore& SnippetStore::instance()
{
    static SnippetStore s;
    return s;
}

QVector<Snippet> SnippetStore::builtinSnippets()
{
    QVector<Snippet> b;
    auto add = [&b](const QString& langs, const QString& trigger, const QString& desc,
                    const QString& body) {
        Snippet s;
        s.trigger = trigger;
        s.body = body;
        s.description = desc;
        if (!langs.isEmpty())
            s.languages = langs.split(u',');
        b.append(s);
    };

    add(QStringLiteral("cpp,c"), QStringLiteral("main"),
        QStringLiteral("main() function"),
        QStringLiteral("int main(int argc, char* argv[])\n{\n\t$0\n}\n"));
    add(QStringLiteral("cpp,c"), QStringLiteral("fori"),
        QStringLiteral("for loop"),
        QStringLiteral("for (int i = 0; i < $0; ++i)\n{\n\t\n}"));
    add(QStringLiteral("cpp"), QStringLiteral("class"),
        QStringLiteral("class with constructor"),
        QStringLiteral("class $0\n{\npublic:\n\t$0() = default;\n};"));
    add(QStringLiteral("cpp,c"), QStringLiteral("ifn"),
        QStringLiteral("if (not)"),
        QStringLiteral("if (!$0)\n\treturn;"));
    add(QStringLiteral("python"), QStringLiteral("main"),
        QStringLiteral("if __name__ == \"__main__\""),
        QStringLiteral("if __name__ == \"__main__\":\n    $0\n"));
    add(QStringLiteral("python"), QStringLiteral("def"),
        QStringLiteral("function definition"),
        QStringLiteral("def $0():\n    pass"));
    add(QStringLiteral("python"), QStringLiteral("forr"),
        QStringLiteral("for loop over range"),
        QStringLiteral("for i in range($0):\n    pass"));
    add(QStringLiteral("javascript,typescript"), QStringLiteral("fun"),
        QStringLiteral("function"),
        QStringLiteral("function $0() {\n\t\n}"));
    add(QStringLiteral("javascript,typescript"), QStringLiteral("arrow"),
        QStringLiteral("arrow function"),
        QStringLiteral("const $0 = () => {\n\t\n};"));
    add(QStringLiteral("javascript,typescript"), QStringLiteral("clog"),
        QStringLiteral("console.log"),
        QStringLiteral("console.log($0);"));
    add(QStringLiteral("json"), QStringLiteral("obj"),
        QStringLiteral("object"),
        QStringLiteral("{\n\t\"$0\": \n}"));
    add(QStringLiteral("html"), QStringLiteral("html5"),
        QStringLiteral("HTML5 skeleton"),
        QStringLiteral("<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n\t<meta charset=\"UTF-8\">\n\t<title>$0</title>\n</head>\n<body>\n\t\n</body>\n</html>\n"));
    add(QStringLiteral("css"), QStringLiteral("flex"),
        QStringLiteral("flexbox container"),
        QStringLiteral("display: flex;\njustify-content: $0;\nalign-items: center;"));
    add(QStringLiteral("cmake"), QStringLiteral("proj"),
        QStringLiteral("cmake project"),
        QStringLiteral("cmake_minimum_required(VERSION 3.21)\nproject($0 VERSION 0.1 LANGUAGES CXX)\n"));
    add(QStringLiteral("markdown"), QStringLiteral("tbl"),
        QStringLiteral("table"),
        QStringLiteral("| Column | Column |\n|--------|--------|\n| $0     |        |\n"));
    add(QStringLiteral("shell"), QStringLiteral("shebang"),
        QStringLiteral("bash shebang"),
        QStringLiteral("#!/usr/bin/env bash\nset -euo pipefail\n\n$0\n"));
    return b;
}

QVector<Snippet> SnippetStore::loadFile(const QString& path)
{
    QVector<Snippet> out;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return out;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    const QJsonArray arr = doc.array();
    for (const auto& v : arr) {
        const QJsonObject o = v.toObject();
        Snippet s;
        s.trigger = o.value(QStringLiteral("trigger")).toString();
        s.body = o.value(QStringLiteral("body")).toString();
        s.description = o.value(QStringLiteral("description")).toString();
        const QJsonArray langs = o.value(QStringLiteral("languages")).toArray();
        for (const auto& l : langs)
            s.languages.append(l.toString());
        if (!s.trigger.isEmpty() && !s.body.isEmpty())
            out.append(s);
    }
    return out;
}

bool SnippetStore::saveUserFile(const QVector<Snippet>& snippets) const
{
    const QString path = userSnippetsPath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QJsonArray arr;
    for (const Snippet& s : snippets) {
        QJsonObject o;
        o.insert(QStringLiteral("trigger"), s.trigger);
        o.insert(QStringLiteral("body"), s.body);
        o.insert(QStringLiteral("description"), s.description);
        QJsonArray langs;
        for (const QString& l : s.languages)
            langs.append(l);
        if (!s.languages.isEmpty())
            o.insert(QStringLiteral("languages"), langs);
        arr.append(o);
    }
    QString err;
    if (!fs::writeAllAtomic(path, QJsonDocument(arr).toJson(QJsonDocument::Indented), &err)) {
        Logger::instance().error(QStringLiteral("SnippetStore: cannot save %1: %2").arg(path, err));
        return false;
    }
    return true;
}

void SnippetStore::load()
{
    m_user = loadFile(userSnippetsPath());
    if (!m_workspaceRoot.isEmpty())
        m_workspace = loadFile(m_workspaceRoot + QStringLiteral("/.codeforge/snippets.json"));
    else
        m_workspace.clear();
    m_loaded = true;
}

void SnippetStore::setWorkspaceRoot(const QString& root)
{
    if (m_workspaceRoot == root)
        return;
    m_workspaceRoot = root;
    load();
    emit changed();
}

QVector<Snippet> SnippetStore::forLanguage(const QString& languageId) const
{
    if (!m_loaded)
        const_cast<SnippetStore*>(this)->load();
    QVector<Snippet> out;
    const auto consider = [&out, &languageId](const QVector<Snippet>& list) {
        for (const Snippet& s : list) {
            if (s.languages.isEmpty() || languageId.isEmpty()
                || s.languages.contains(languageId))
                out.append(s);
        }
    };
    consider(builtinSnippets());
    consider(m_user);
    consider(m_workspace);
    return out;
}

QVector<CompletionItem> SnippetStore::completionItems(const QString& languageId) const
{
    QVector<CompletionItem> items;
    const QVector<Snippet> snips = forLanguage(languageId);
    items.reserve(snips.size());
    for (const Snippet& s : snips) {
        CompletionItem it;
        it.label = s.trigger;
        it.insertText = s.trigger;
        it.detail = s.description.isEmpty() ? QStringLiteral("snippet")
                                            : QStringLiteral("snippet: %1").arg(s.description);
        it.kind = 4;
        it.isSnippet = true;
        it.snippetBody = s.body;
        items.append(it);
    }
    return items;
}

bool SnippetStore::expandAtCursor(CodeEditor* editor, const QString& body)
{
    if (!editor)
        return false;
    // The trigger word before the cursor is removed; the body (with $0 as
    // caret marker) is inserted in its place.
    QTextCursor c = editor->textCursor();
    const QString blockText = c.block().text();
    const int col = c.positionInBlock();
    int start = col;
    while (start > 0
           && (blockText.at(start - 1).isLetterOrNumber() || blockText.at(start - 1) == u'_'))
        --start;
    const QString word = blockText.mid(start, col - start);
    if (word.isEmpty())
        return false;
    c.setPosition(c.position() - word.size(), QTextCursor::KeepAnchor);
    c.removeSelectedText();

    QString text = body;
    int marker = text.indexOf(QLatin1String("$0"));
    int caretOffset = -1;
    if (marker >= 0) {
        caretOffset = marker;
        text.remove(marker, 2);
    }
    c.insertText(text);
    if (caretOffset >= 0)
        c.setPosition(c.position() - text.size() + caretOffset);
    editor->setTextCursor(c);
    return true;
}

bool SnippetStore::expandIfMatch(CodeEditor* editor, const QString& languageId,
                                 const QString& trigger) const
{
    const QVector<Snippet> snips = forLanguage(languageId);
    for (const Snippet& s : snips) {
        if (s.trigger == trigger)
            return expandAtCursor(editor, s.body);
    }
    return false;
}

QVector<Snippet> SnippetStore::userSnippets() const
{
    if (!m_loaded)
        const_cast<SnippetStore*>(this)->load();
    return m_user;
}

void SnippetStore::setUserSnippets(const QVector<Snippet>& snippets)
{
    m_user = snippets;
    saveUserFile(snippets);
    emit changed();
}

QString SnippetStore::userSnippetsPath() const
{
    return paths::dataRoot() + QStringLiteral("/snippets.json");
}

}  // namespace cf
