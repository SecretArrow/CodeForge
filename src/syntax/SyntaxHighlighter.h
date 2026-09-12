#pragma once
// Multi-language syntax highlighter engine (QSyntaxHighlighter-based).
// Rules are built per language from a compact definition table; multi-line
// constructs (C block comments, Python triple strings) use block states.
// Colors come from the active theme token set.
#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>

namespace cf {

class Theme;

enum class Tok {
    Keyword, Control, String, Comment, Number, Function, Type, Variable,
    Constant, Operator, Preprocessor, Tag, Attribute, Property, Heading, Link
};

class SyntaxHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit SyntaxHighlighter(QTextDocument* doc);
    void setLanguage(const QString& languageId);   // "" = plain text
    void applyTheme(const Theme& theme);

protected:
    void highlightBlock(const QString& text) override;

private:
    struct Rule {
        QRegularExpression re;
        int group = 0;
        Tok token = Tok::Keyword;
    };
    struct MultilineDef {
        QRegularExpression start;
        QRegularExpression end;
        Tok token = Tok::Comment;
    };

    void buildRules(const QString& languageId);
    void addWordRule(const QStringList& words, Tok token);
    void addRule(const QString& pattern, int group, Tok token);
    QTextCharFormat formatFor(Tok token) const;
    void highlightMultiline(const QString& text, int& state);

    QList<Rule> m_rules;
    QList<MultilineDef> m_multiline;
    QString m_language;
    QHash<int, QTextCharFormat> m_formats;   // Tok -> format (rebuilt on theme)
};

}  // namespace cf
