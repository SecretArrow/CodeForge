#pragma once
// Language Server Protocol abstraction layer.
//
// Phase-1 ships a NullLanguageService (no features). This interface is the
// seam where a real LSP client (Phase 4+) attaches without touching the
// editor: diagnostics, go-to-definition, hover. Registered per language id
// via LanguageServiceRegistry.
#include <QHash>
#include <QString>
#include <QVector>

namespace cf {

class TextDocument;

struct Diagnostic {
    enum Severity { Hint, Info, Warning, Error } severity = Info;
    int line = 0;
    int column = 0;
    int length = 0;
    QString message;
    QString source;
};

struct SymbolRange {
    int startLine = 0;
    int startCol = 0;
    int endLine = 0;
    int endCol = 0;
};

class LanguageService {
public:
    virtual ~LanguageService() = default;

    virtual QString name() const = 0;
    virtual bool isActive() const { return false; }

    virtual QVector<Diagnostic> diagnostics(TextDocument*) const { return {}; }
    virtual bool goToDefinition(TextDocument*, int /*line*/, int /*col*/, SymbolRange* /*out*/) { return false; }
    virtual QString hover(TextDocument*, int /*line*/, int /*col*/) { return QString(); }
};

// Default implementation: everything disabled, used when no LSP is attached.
class NullLanguageService : public LanguageService {
public:
    QString name() const override { return QStringLiteral("none"); }
};

class LanguageServiceRegistry {
public:
    static LanguageServiceRegistry& instance();

    void registerService(const QString& languageId, LanguageService* svc);
    LanguageService* serviceFor(const QString& languageId);

private:
    LanguageServiceRegistry() = default;
    QHash<QString, LanguageService*> m_services;
    NullLanguageService m_null;
};

}  // namespace cf
