#pragma once
// Lightweight regex-based symbol scanner (functions/classes/defs) used for
// the Outline panel, Quick Open "@" mode and breadcrumbs.
// This is NOT a full language parser; the LSP abstraction (lsp/) supersedes it
// once language servers are attached.
#include <QList>
#include <QString>

namespace cf {

struct SymbolInfo {
    QString name;
    QString kind;    // "function", "class", "struct", "namespace", "method", "variable"
    int line = 0;    // 0-based
    int indent = 0;
};

class SymbolScanner {
public:
    static QList<SymbolInfo> scan(const QString& languageId, const QString& text);
};

}  // namespace cf
