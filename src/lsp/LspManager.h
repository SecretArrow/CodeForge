#pragma once
// LspManager: one LspClient per configured language, lifecycle handled from
// document open/close/change events. Server commands come from settings:
//   lsp.enabled  (bool)
//   lsp.servers  (string list, entries "languageId=command [args...]")
// e.g.  cpp=clangd
//       python=pyright-langserver --stdio
// Diagnostics are cached here and re-emitted per file.
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVector>

#include "lsp/LanguageService.h"

namespace cf {

class LspClient;
class TextDocument;

class LspManager : public QObject {
    Q_OBJECT
public:
    explicit LspManager(QObject* parent = nullptr);

    // Reads lsp.* settings and (re)starts/stops clients as needed.
    void configure(const QString& workspaceRoot);

    // Document lifecycle hooks (wired by MainWindow).
    void handleDocumentOpened(TextDocument* doc);
    void handleDocumentClosed(const QString& docId, const QString& path);

    // Async language features.
    void hover(TextDocument* doc, int line, int col, std::function<void(const QString& html)> cb);
    void definition(TextDocument* doc, int line, int col,
                    std::function<void(const QString& path, int line, int col)> cb);

    bool hasDiagnostics(const QString& path) const;
    QVector<Diagnostic> diagnosticsFor(const QString& path) const;
    bool isActive() const { return !m_clients.isEmpty(); }

signals:
    // Emitted after any change to a file's diagnostics.
    void diagnosticsUpdated(const QString& path);

private slots:
    void onDiagnostics(const QString& path, const QVector<cf::Diagnostic>& diags);
    void onServerExited(const QString& languageId);

private:
    LspClient* clientForLanguage(const QString& languageId) const;
    void startClient(const QString& languageId, const QString& command, const QStringList& args);
    void ensureStarted(TextDocument* doc);

    QHash<QString, LspClient*> m_clients;                 // languageId -> client
    QHash<QString, QVector<Diagnostic>> m_diagnostics;    // path -> latest diags
    QHash<TextDocument*, QTimer*> m_changeTimers;   // raw key: QPointer lacks qHash
    QString m_workspaceRoot;
};

}  // namespace cf
