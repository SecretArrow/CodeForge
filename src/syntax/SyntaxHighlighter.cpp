#include "syntax/SyntaxHighlighter.h"

#include "themes/Theme.h"

namespace cf {

using Qt::CaseInsensitive;

SyntaxHighlighter::SyntaxHighlighter(QTextDocument* doc) : QSyntaxHighlighter(doc) {}

void SyntaxHighlighter::setLanguage(const QString& languageId)
{
    if (m_language == languageId) return;
    m_language = languageId;
    buildRules(languageId);
    rehighlight();
}

void SyntaxHighlighter::applyTheme(const Theme& theme)
{
    m_formats.clear();
    static const QList<Tok> toks = {
        Tok::Keyword, Tok::Control, Tok::String, Tok::Comment, Tok::Number, Tok::Function,
        Tok::Type, Tok::Variable, Tok::Constant, Tok::Operator, Tok::Preprocessor, Tok::Tag,
        Tok::Attribute, Tok::Property, Tok::Heading, Tok::Link
    };
    static const QHash<Tok, QString> names = {
        { Tok::Keyword, QStringLiteral("keyword") }, { Tok::Control, QStringLiteral("control") },
        { Tok::String, QStringLiteral("string") }, { Tok::Comment, QStringLiteral("comment") },
        { Tok::Number, QStringLiteral("number") }, { Tok::Function, QStringLiteral("function") },
        { Tok::Type, QStringLiteral("type") }, { Tok::Variable, QStringLiteral("variable") },
        { Tok::Constant, QStringLiteral("constant") }, { Tok::Operator, QStringLiteral("operator") },
        { Tok::Preprocessor, QStringLiteral("preprocessor") }, { Tok::Tag, QStringLiteral("tag") },
        { Tok::Attribute, QStringLiteral("attribute") }, { Tok::Property, QStringLiteral("property") },
        { Tok::Heading, QStringLiteral("heading") }, { Tok::Link, QStringLiteral("link") }
    };
    for (Tok t : toks) {
        QTextCharFormat f;
        f.setForeground(theme.syntaxColor(names.value(t)));
        m_formats.insert(int(t), f);
    }
    rehighlight();
}

QTextCharFormat SyntaxHighlighter::formatFor(Tok token) const
{
    return m_formats.value(int(token));
}

void SyntaxHighlighter::addRule(const QString& pattern, int group, Tok token)
{
    Rule r;
    r.re = QRegularExpression(pattern);
    r.group = group;
    r.token = token;
    m_rules.append(r);
}

void SyntaxHighlighter::addWordRule(const QStringList& words, Tok token)
{
    if (words.isEmpty()) return;
    const QString pattern = QStringLiteral("\\b(?:%1)\\b").arg(words.join(QLatin1Char('|')));
    addRule(pattern, 0, token);
}

void SyntaxHighlighter::buildRules(const QString& lang)
{
    m_rules.clear();
    m_multiline.clear();
    if (lang.isEmpty()) return;

    const QStringList cKeywords = {
        "alignas","alignof","auto","bool","break","case","catch","char","char8_t","char16_t","char32_t",
        "class","concept","const","consteval","constexpr","constinit","const_cast","continue","co_await",
        "co_return","co_yield","decltype","default","delete","do","double","dynamic_cast","else","enum",
        "explicit","export","extern","false","final","float","for","friend","goto","if","inline","int",
        "long","mutable","namespace","new","noexcept","nullptr","operator","override","private","protected",
        "public","register","reinterpret_cast","requires","return","short","signed","sizeof","static",
        "static_assert","static_cast","struct","switch","template","this","thread_local","throw","true",
        "try","typedef","typeid","typename","union","unsigned","using","virtual","void","volatile",
        "wchar_t","while","override","final"
    };

    if (lang == QLatin1String("cpp") || lang == QLatin1String("c")) {
        addWordRule(cKeywords, Tok::Keyword);
        addWordRule({"if","else","for","while","switch","case","default","return","break","continue",
                     "goto","try","catch","throw","co_await","co_return","co_yield"}, Tok::Control);
        addRule(QStringLiteral("//[^\n]*"), 0, Tok::Comment);
        addRule(QStringLiteral("#\\s*\\w+.*"), 0, Tok::Preprocessor);
        addRule(QStringLiteral("\"(\\\\.|[^\"\\\\])*\""), 0, Tok::String);
        addRule(QStringLiteral("'(\\\\.|[^'\\\\])*'"), 0, Tok::String);
        addRule(QStringLiteral("\\b0[xX][0-9a-fA-F']+\\b|\\b\\d[\\d'.]*(?:[eE][+-]?\\d+)?[fFuUlL]*\\b"), 0, Tok::Number);
        addRule(QStringLiteral("\\b([A-Za-z_]\\w*)\\s*\\("), 1, Tok::Function);
        addRule(QStringLiteral("\\b[A-Z][A-Za-z0-9_]*\\b"), 0, Tok::Type);
        addRule(QStringLiteral("\\b[a-z][A-Za-z0-9_]*(?=_t)\\b"), 0, Tok::Type);
        addRule(QStringLiteral("\\b[A-Z_][A-Z0-9_]{2,}\\b"), 0, Tok::Constant);
        m_multiline.append({ QRegularExpression(QStringLiteral("/\\*")),
                             QRegularExpression(QStringLiteral("\\*/")), Tok::Comment });
    } else if (lang == QLatin1String("csharp") || lang == QLatin1String("java")) {
        addWordRule(cKeywords, Tok::Keyword);
        addWordRule({"public","private","protected","internal","static","sealed","abstract","virtual",
                     "override","interface","implements","extends","package","import","using","namespace",
                     "record","sealed","readonly","ref","out","in","params","var","dynamic","async","await",
                     "yield","get","set","value","new","base","super","is","as","lock","synchronized"}, Tok::Control);
        addRule(QStringLiteral("//[^\n]*"), 0, Tok::Comment);
        addRule(QStringLiteral("@\\w+"), 0, Tok::Attribute);
        addRule(QStringLiteral("\"(\\\\.|[^\"\\\\])*\""), 0, Tok::String);
        addRule(QStringLiteral("\\b\\d[\\d_]*(?:\\.\\d+)?[fFdDlLmM]?\\b"), 0, Tok::Number);
        addRule(QStringLiteral("\\b([A-Za-z_]\\w*)\\s*\\("), 1, Tok::Function);
        addRule(QStringLiteral("\\b[A-Z][A-Za-z0-9_]*\\b"), 0, Tok::Type);
        m_multiline.append({ QRegularExpression(QStringLiteral("/\\*")),
                             QRegularExpression(QStringLiteral("\\*/")), Tok::Comment });
    } else if (lang == QLatin1String("javascript") || lang == QLatin1String("typescript")) {
        addWordRule({"var","let","const","function","class","extends","constructor","this","super","new",
                     "delete","typeof","instanceof","in","of","import","export","from","default","async",
                     "await","yield","static","get","set","interface","type","enum","implements","public",
                     "private","protected","readonly","declare","namespace","abstract","as","satisfies",
                     "undefined","null","NaN"}, Tok::Keyword);
        addWordRule({"if","else","for","while","do","switch","case","break","continue","return","try",
                     "catch","finally","throw"}, Tok::Control);
        addRule(QStringLiteral("//[^\n]*"), 0, Tok::Comment);
        addRule(QStringLiteral("`(?:\\\\.|[^`\\\\])*`"), 0, Tok::String);
        addRule(QStringLiteral("\"(?:\\\\.|[^\"\\\\])*\"|'(?:\\\\.|[^'\\\\])*'"), 0, Tok::String);
        addRule(QStringLiteral("\\b\\d[\\d_]*(?:\\.\\d+)?\\b"), 0, Tok::Number);
        addRule(QStringLiteral("\\b([A-Za-z_$][\\w$]*)\\s*(?=\\()"), 1, Tok::Function);
        addRule(QStringLiteral("\\b[A-Z][A-Za-z0-9_]*\\b"), 0, Tok::Type);
        addRule(QStringLiteral("\\btrue\\b|\\bfalse\\b"), 0, Tok::Constant);
        m_multiline.append({ QRegularExpression(QStringLiteral("/\\*")),
                             QRegularExpression(QStringLiteral("\\*/")), Tok::Comment });
    } else if (lang == QLatin1String("python")) {
        addWordRule({"def","class","lambda","global","nonlocal","import","from","as","pass","with","is",
                     "not","and","or","in","del","async","await","None","True","False","self","cls","raise",
                     "assert","match","case","yield"}, Tok::Keyword);
        addWordRule({"if","elif","else","for","while","break","continue","return","try","except","finally",
                     "throw"}, Tok::Control);
        addRule(QStringLiteral("#[^\n]*"), 0, Tok::Comment);
        addRule(QStringLiteral("@\\w+"), 0, Tok::Attribute);
        addRule(QStringLiteral("\\b\\d[\\d_]*(?:\\.\\d+)?[jJ]?\\b|0[xX][0-9a-fA-F_]+\\b|0[bB][01_]+\\b"), 0, Tok::Number);
        addRule(QStringLiteral("\"\"\"|'''"), 0, Tok::String);
        addRule(QStringLiteral("\\b([A-Za-z_]\\w*)\\s*(?=\\()"), 1, Tok::Function);
        addRule(QStringLiteral("\\b[A-Z][A-Za-z0-9_]*\\b"), 0, Tok::Type);
        m_multiline.append({ QRegularExpression(QStringLiteral("\"\"\"")),
                             QRegularExpression(QStringLiteral("\"\"\"")), Tok::String });
        m_multiline.append({ QRegularExpression(QStringLiteral("'''")),
                             QRegularExpression(QStringLiteral("'''")), Tok::String });
    } else if (lang == QLatin1String("rust")) {
        addWordRule({"as","async","await","break","const","continue","crate","dyn","enum","extern","false",
                     "fn","for","impl","in","let","loop","match","mod","move","mut","pub","ref","return",
                     "self","Self","static","struct","super","trait","true","type","unsafe","use","where",
                     "while"}, Tok::Keyword);
        addRule(QStringLiteral("//[^\n]*"), 0, Tok::Comment);
        addRule(QStringLiteral("!\\b\\w+\\b|#!\\[\\w+|\\[\\w+\\("), 0, Tok::Attribute);
        addRule(QStringLiteral("\\b\\d[\\d_]*(?:\\.\\d+)?(?:[iuf](?:8|16|32|64|size))?\\b"), 0, Tok::Number);
        addRule(QStringLiteral("\"(?:\\\\.|[^\"\\\\])*\""), 0, Tok::String);
        addRule(QStringLiteral("\\b([a-z_]\\w*)\\s*(?=\\()"), 1, Tok::Function);
        addRule(QStringLiteral("\\b[A-Z][A-Za-z0-9_]*\\b"), 0, Tok::Type);
        m_multiline.append({ QRegularExpression(QStringLiteral("/\\*")),
                             QRegularExpression(QStringLiteral("\\*/")), Tok::Comment });
    } else if (lang == QLatin1String("go")) {
        addWordRule({"break","case","chan","const","continue","default","defer","else","fallthrough",
                     "for","func","go","goto","if","import","interface","map","package","range","return",
                     "select","struct","switch","type","var","nil","true","false","iota"}, Tok::Keyword);
        addRule(QStringLiteral("//[^\n]*"), 0, Tok::Comment);
        addRule(QStringLiteral("`[^`]*`"), 0, Tok::String);
        addRule(QStringLiteral("\"(?:\\\\.|[^\"\\\\])*\""), 0, Tok::String);
        addRule(QStringLiteral("\\b\\d[\\d_]*(?:\\.\\d+)?\\b"), 0, Tok::Number);
        addRule(QStringLiteral("\\b([a-zA-Z_]\\w*)\\s*(?=\\()"), 1, Tok::Function);
        addRule(QStringLiteral("\\b[A-Z][A-Za-z0-9_]*\\b"), 0, Tok::Type);
        m_multiline.append({ QRegularExpression(QStringLiteral("/\\*")),
                             QRegularExpression(QStringLiteral("\\*/")), Tok::Comment });
    } else if (lang == QLatin1String("php")) {
        addWordRule({"abstract","and","array","as","break","callable","case","catch","class","clone",
                     "const","continue","declare","default","do","echo","else","elseif","enddeclare",
                     "endfor","endforeach","endif","endswitch","endwhile","enum","extends","final",
                     "finally","fn","for","foreach","function","global","goto","if","implements",
                     "include","instanceof","interface","isset","list","match","namespace","new","or",
                     "print","private","protected","public","readonly","require","return","static",
                     "switch","throw","trait","try","unset","use","var","while","xor","yield","true",
                     "false","null"}, Tok::Keyword);
        addRule(QStringLiteral("//[^\n]*|#(?![\\[!])[^\n]*"), 0, Tok::Comment);
        addRule(QStringLiteral("\\$\\w+"), 0, Tok::Variable);
        addRule(QStringLiteral("\"(?:\\\\.|[^\"\\\\])*\"|'(?:[^'])*'"), 0, Tok::String);
        addRule(QStringLiteral("\\b\\d+(?:\\.\\d+)?\\b"), 0, Tok::Number);
        addRule(QStringLiteral("\\b([A-Za-z_]\\w*)\\s*\\("), 1, Tok::Function);
        m_multiline.append({ QRegularExpression(QStringLiteral("/\\*")),
                             QRegularExpression(QStringLiteral("\\*/")), Tok::Comment });
    } else if (lang == QLatin1String("html") || lang == QLatin1String("xml")) {
        addRule(QStringLiteral("<!--[^\n]*"), 0, Tok::Comment);
        addRule(QStringLiteral("(</?)([A-Za-z][\\w:-]*)"), 2, Tok::Tag);
        addRule(QStringLiteral("([\\w:-]+)(?==)"), 1, Tok::Attribute);
        addRule(QStringLiteral("\"[^\"]*\"|'[^']*'"), 0, Tok::String);
        addRule(QStringLiteral("&\\w+;"), 0, Tok::Constant);
    } else if (lang == QLatin1String("css")) {
        addRule(QStringLiteral("/\\*[^\n]*"), 0, Tok::Comment);
        addRule(QStringLiteral("(#[0-9a-fA-F]{3,8}|\\b\\d+(?:\\.\\d+)?(?:px|em|rem|%|vh|vw|s|ms)?\\b)"), 1, Tok::Number);
        addRule(QStringLiteral("\\.[-\\w]+|#[-\\w]+"), 0, Tok::Type);
        addRule(QStringLiteral("([\\w-]+)\\s*:"), 1, Tok::Property);
        addRule(QStringLiteral("@\\w+"), 0, Tok::Control);
        addRule(QStringLiteral("\"[^\"]*\"|'[^']*'"), 0, Tok::String);
        addRule(QStringLiteral("--[\\w-]+"), 0, Tok::Variable);
    } else if (lang == QLatin1String("json")) {
        addRule(QStringLiteral("(\"(?:\\\\.|[^\"\\\\])*\")(?=\\s*:)"), 1, Tok::Property);
        addRule(QStringLiteral("\"(?:\\\\.|[^\"\\\\])*\""), 0, Tok::String);
        addRule(QStringLiteral("-?\\b\\d+(?:\\.\\d+)?(?:[eE][+-]?\\d+)?\\b"), 0, Tok::Number);
        addRule(QStringLiteral("\\btrue\\b|\\bfalse\\b|\\bnull\\b"), 0, Tok::Constant);
        addRule(QStringLiteral("//[^\n]*"), 0, Tok::Comment);
    } else if (lang == QLatin1String("yaml")) {
        addRule(QStringLiteral("#[^\n]*"), 0, Tok::Comment);
        addRule(QStringLiteral("(^[ \\t]*[\\w.-]+)(?=\\s*:)"), 1, Tok::Property);
        addRule(QStringLiteral("\"[^\"]*\"|'[^']*'"), 0, Tok::String);
        addRule(QStringLiteral("\\b\\d+(?:\\.\\d+)?\\b"), 0, Tok::Number);
        addRule(QStringLiteral("\\btrue\\b|\\bfalse\\b|\\bnull\\b|\\b~\\b"), 0, Tok::Constant);
        addRule(QStringLiteral("^[ \\t]*-\\s"), 0, Tok::Operator);
    } else if (lang == QLatin1String("markdown")) {
        addRule(QStringLiteral("^#{1,6}\\s.*"), 0, Tok::Heading);
        addRule(QStringLiteral("\\*\\*[^*\n]+\\*\\*|__[^_\n]+__"), 0, Tok::Keyword);
        addRule(QStringLiteral("\\*[^*\n]+\\*|_[^_\n]+_"), 0, Tok::Property);
        addRule(QStringLiteral("`[^`\n]+`|```[^\n]*"), 0, Tok::String);
        addRule(QStringLiteral("\\[([^\\]\\n]*)\\]\\(([^)\\n]*)\\)"), 2, Tok::Link);
        addRule(QStringLiteral("^>[^\\n]*"), 0, Tok::Comment);
        addRule(QStringLiteral("^[*+-]\\s|^[0-9]+\\.\\s"), 0, Tok::Control);
    } else if (lang == QLatin1String("sql")) {
        addWordRule({"SELECT","FROM","WHERE","INSERT","INTO","VALUES","UPDATE","SET","DELETE","CREATE",
                     "TABLE","DATABASE","INDEX","VIEW","DROP","ALTER","ADD","JOIN","INNER","LEFT","RIGHT",
                     "FULL","OUTER","ON","GROUP","BY","ORDER","HAVING","LIMIT","OFFSET","AS","AND","OR",
                     "NOT","NULL","IS","IN","BETWEEN","LIKE","EXISTS","UNION","ALL","DISTINCT","PRIMARY",
                     "KEY","FOREIGN","REFERENCES","DEFAULT","CONSTRAINT","TRIGGER","PROCEDURE","BEGIN",
                     "END","COMMIT","ROLLBACK","CASE","WHEN","THEN","ELSE","IF"}, Tok::Keyword);
        addRule(QStringLiteral("--[^\n]*"), 0, Tok::Comment);
        addRule(QStringLiteral("'(?:''|[^'])*'"), 0, Tok::String);
        addRule(QStringLiteral("\\b\\d+(?:\\.\\d+)?\\b"), 0, Tok::Number);
        m_multiline.append({ QRegularExpression(QStringLiteral("/\\*")),
                             QRegularExpression(QStringLiteral("\\*/")), Tok::Comment });
    } else if (lang == QLatin1String("shell")) {
        addWordRule({"if","then","elif","else","fi","for","in","do","done","while","until","case",
                     "esac","function","return","break","continue","exit","local","export","readonly",
                     "set","unset","shift","source","alias","trap","eval","exec","echo","printf",
                     "test","select","time","declare","typeset"}, Tok::Keyword);
        addRule(QStringLiteral("#[^\n]*"), 0, Tok::Comment);
        addRule(QStringLiteral("\\$\\{?[\\w@#?*!$-]+\\}?"), 0, Tok::Variable);
        addRule(QStringLiteral("\"(?:\\\\.|[^\"\\\\])*\"|'[^']*'"), 0, Tok::String);
        addRule(QStringLiteral("\\b\\d+\\b"), 0, Tok::Number);
        addRule(QStringLiteral("^\\s*(\\w+)(?=\\s+\\S)"), 1, Tok::Function);
    } else if (lang == QLatin1String("powershell")) {
        addWordRule({"if","elseif","else","foreach","for","while","do","switch","break","continue",
                     "return","throw","try","catch","finally","function","filter","param","begin","process",
                     "end","in","class","enum","using","module","exit","trap","data","dynamicparam"}, Tok::Keyword);
        addRule(QStringLiteral("#[^\n]*"), 0, Tok::Comment);
        addRule(QStringLiteral("\\$\\w+"), 0, Tok::Variable);
        addRule(QStringLiteral("(?:-[A-Za-z]\\w*)"), 0, Tok::Operator);
        addRule(QStringLiteral("\"(?:\\\\.|[^\"\\\\])*\"|'(?:[^'])*'"), 0, Tok::String);
        addRule(QStringLiteral("\\b([A-Za-z]-[A-Za-z][\\w-]*)\\b"), 1, Tok::Function);
        addRule(QStringLiteral("\\b\\d+(?:\\.\\d+)?\\b"), 0, Tok::Number);
        m_multiline.append({ QRegularExpression(QStringLiteral("<#")),
                             QRegularExpression(QStringLiteral("#>")), Tok::Comment });
    } else if (lang == QLatin1String("cmake")) {
        addWordRule({"if","else","elseif","endif","foreach","endforeach","while","endwhile","function",
                     "endfunction","macro","endmacro","set","unset","include","project","add_executable",
                     "add_library","target_link_libraries","target_include_directories","target_compile_definitions",
                     "find_package","find_library","find_path","message","option","cmake_minimum_required",
                     "set_target_properties","add_subdirectory","enable_testing","install","file","string",
                     "list","math","get_filename_component","execute_process","configure_file"}, Tok::Keyword);
        addRule(QStringLiteral("#[^\n]*"), 0, Tok::Comment);
        addRule(QStringLiteral("\\$\\{[^}]*\\}|\\$<[^>]*>"), 0, Tok::Variable);
        addRule(QStringLiteral("\"[^\"]*\""), 0, Tok::String);
        addRule(QStringLiteral("\\b([A-Za-z_][\\w]*)\\s*\\("), 1, Tok::Function);
        addRule(QStringLiteral("\\b\\d+(?:\\.\\d+)?\\b"), 0, Tok::Number);
    } else if (lang == QLatin1String("ini") || lang == QLatin1String("toml")) {
        addRule(QStringLiteral("[#;][^\n]*"), 0, Tok::Comment);
        addRule(QStringLiteral("^\\s*\\[[^\\]]*\\]"), 0, Tok::Type);
        addRule(QStringLiteral("(^[\\w.$-]+)(?=\\s*[=:])"), 1, Tok::Property);
        addRule(QStringLiteral("\"[^\"]*\"|'[^']*'"), 0, Tok::String);
        addRule(QStringLiteral("\\b\\d+(?:\\.\\d+)?\\b|\\btrue\\b|\\bfalse\\b"), 0, Tok::Number);
    }
}

void SyntaxHighlighter::highlightBlock(const QString& text)
{
    const int prevState = previousBlockState();
    int state = 0;  // -1 default; >0 == inside multiline def (state-1)
    setCurrentBlockState(-1);

    int startIdx = 0;
    if (prevState >= 0 && prevState < m_multiline.size() + 1 && prevState > 0) {
        // Continuation of a multiline construct.
        highlightMultiline(text, state);
        if (state > 0) { setCurrentBlockState(state); return; }
        // Construct ended inside this block: find where.
        const MultilineDef& def = m_multiline.at(prevState - 1);
        const QRegularExpressionMatch endMatch = def.end.match(text);
        if (endMatch.hasMatch()) startIdx = endMatch.capturedEnd();
        else { setCurrentBlockState(prevState); return; }
    }

    // Single-line rules over the remaining region.
    for (const Rule& rule : m_rules) {
        QRegularExpressionMatchIterator it = rule.re.globalMatch(text, startIdx);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            const int g = qBound(0, rule.group, m.lastCapturedIndex());
            if (m.capturedStart(g) < 0) continue;
            setFormat(m.capturedStart(g), m.capturedLength(g), formatFor(rule.token));
        }
    }

    // Enter a multiline construct from this block.
    if (!m_multiline.isEmpty()) {
        for (int d = 0; d < m_multiline.size(); ++d) {
            const MultilineDef& def = m_multiline.at(d);
            const QRegularExpressionMatch startMatch = def.start.match(text, startIdx);
            if (!startMatch.hasMatch()) continue;
            setFormat(startMatch.capturedStart(), text.length() - startMatch.capturedStart(), formatFor(def.token));
            setCurrentBlockState(d + 1);
            return;
        }
    }
}

void SyntaxHighlighter::highlightMultiline(const QString& text, int& state)
{
    // Used at block start when previous block ended inside a multiline region.
    const int defIndex = previousBlockState() - 1;
    if (defIndex < 0 || defIndex >= m_multiline.size()) return;
    const MultilineDef& def = m_multiline.at(defIndex);
    const QRegularExpressionMatch endMatch = def.end.match(text);
    if (endMatch.hasMatch()) {
        setFormat(0, endMatch.capturedEnd(), formatFor(def.token));
        state = 0;
    } else {
        setFormat(0, text.length(), formatFor(def.token));
        state = defIndex + 1;
    }
}

}  // namespace cf
