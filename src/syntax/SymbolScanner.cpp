#include "syntax/SymbolScanner.h"

#include <QRegularExpression>

namespace cf {

namespace {

struct Scanner {
    QRegularExpression re;
    const char* kind;
    int nameGroup;
};

QList<Scanner> scannersFor(const QString& lang)
{
    QList<Scanner> s;
    const QString word = QStringLiteral("[A-Za-z_]\\w*");

    if (lang == QLatin1String("cpp") || lang == QLatin1String("c") || lang == QLatin1String("csharp") ||
        lang == QLatin1String("java") || lang == QLatin1String("javascript") || lang == QLatin1String("typescript") ||
        lang == QLatin1String("rust") || lang == QLatin1String("go") || lang == QLatin1String("php")) {
        s.append({ QRegularExpression(QStringLiteral("^[ \\t]*((?:class|struct|interface|enum|impl|trait|namespace)\\s+)(%1)").arg(word)), "class", 2 });
        s.append({ QRegularExpression(QStringLiteral("^[ \\t]*[\\w:<>&*\\s]+?\\b(%1)\\s*\\([^;{]*\\)\\s*(?:const\\s*)?\\{?\\s*$").arg(word)), "function", 1 });
        s.append({ QRegularExpression(QStringLiteral("^[ \\t]*(?:def|fn|func)\\s+(%1)").arg(word)), "function", 1 });
    }
    if (lang == QLatin1String("python")) {
        s.append({ QRegularExpression(QStringLiteral("^[ \\t]*class\\s+(%1)").arg(word)), "class", 1 });
        s.append({ QRegularExpression(QStringLiteral("^[ \\t]*def\\s+(%1)").arg(word)), "function", 1 });
    }
    if (lang == QLatin1String("javascript") || lang == QLatin1String("typescript")) {
        s.append({ QRegularExpression(QStringLiteral("^[ \\t]*(?:export\\s+)?(?:default\\s+)?class\\s+(%1)").arg(word)), "class", 1 });
        s.append({ QRegularExpression(QStringLiteral("^[ \\t]*(?:export\\s+)?(?:async\\s+)?function\\s+(%1)").arg(word)), "function", 1 });
        s.append({ QRegularExpression(QStringLiteral("^[ \\t]*(?:export\\s+)?(?:const|let|var)\\s+(%1)\\s*=\\s*(?:async\\s*)?(?:\\([^)]*\\)|[\\w$]+)\\s*=>").arg(word)), "function", 1 });
    }
    if (lang == QLatin1String("go")) {
        s.append({ QRegularExpression(QStringLiteral("^type\\s+(%1)\\s+(?:struct|interface)").arg(word)), "class", 1 });
        s.append({ QRegularExpression(QStringLiteral("^func\\s+(?:\\([^)]*\\)\\s*)?(%1)\\s*\\(").arg(word)), "function", 1 });
    }
    if (lang == QLatin1String("rust")) {
        s.append({ QRegularExpression(QStringLiteral("^\\s*(?:pub\\s+)?(?:struct|enum|trait)\\s+(%1)").arg(word)), "class", 1 });
        s.append({ QRegularExpression(QStringLiteral("^\\s*(?:pub\\s+)?fn\\s+(%1)").arg(word)), "function", 1 });
    }
    if (lang == QLatin1String("markdown")) {
        s.append({ QRegularExpression(QStringLiteral("^(#{1,6})\\s+(.+)$")), "heading", 2 });
    }
    return s;
}

}  // namespace

QList<SymbolInfo> SymbolScanner::scan(const QString& languageId, const QString& text)
{
    QList<SymbolInfo> out;
    if (languageId.isEmpty() || text.isEmpty()) return out;
    if (text.size() > 4 * 1024 * 1024) return out;  // skip huge docs

    const QList<Scanner> scanners = scannersFor(languageId);
    const QStringList lines = text.split(QLatin1Char('\n'));

    for (int i = 0; i < lines.size() && i < 200000; ++i) {
        const QString& line = lines.at(i);
        for (const Scanner& sc : scanners) {
            const QRegularExpressionMatch m = sc.re.match(line);
            if (m.hasMatch()) {
                SymbolInfo sym;
                sym.name = m.captured(sc.nameGroup).trimmed();
                sym.kind = QString::fromLatin1(sc.kind);
                sym.line = i;
                sym.indent = m.capturedStart(0);
                if (sc.kind == QString::fromLatin1("heading"))
                    sym.indent = m.captured(1).size() - 1;
                if (!sym.name.isEmpty()) out.append(sym);
                break;
            }
        }
    }
    return out;
}

}  // namespace cf
