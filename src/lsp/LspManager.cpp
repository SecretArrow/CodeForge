#include "lsp/LspManager.h"

#include <QJsonArray>
#include <QProcess>
#include <QTextDocument>

#include "core/Logger.h"
#include "core/TextDocument.h"
#include "lsp/LspClient.h"
#include "lsp/LspProtocol.h"
#include "settings/SettingsManager.h"

namespace cf {

LspManager::LspManager(QObject* parent)
    : QObject(parent)
{
}

void LspManager::configure(const QString& workspaceRoot)
{
    m_workspaceRoot = workspaceRoot;
    SettingsManager& s = SettingsManager::instance();
    if (!s.getBool(QStringLiteral("lsp.enabled"))) {
        for (LspClient* c : std::as_const(m_clients)) c->stop();
        return;
    }

    const QStringList servers = s.get(QStringLiteral("lsp.servers")).toStringList();
    for (const QString& entryRaw : servers) {
        const QString entry = entryRaw.trimmed();
        if (entry.isEmpty()) continue;
        const int eq = entry.indexOf(QLatin1Char('='));
        if (eq <= 0) {
            CF_LOG_WARNING(QStringLiteral("LSP: invalid server entry '%1' (expected language=command)").arg(entry));
            continue;
        }
        const QString lang = entry.left(eq).trimmed();
        const QString cmdline = entry.mid(eq + 1).trimmed();
        if (lang.isEmpty() || cmdline.isEmpty()) continue;

        const QStringList parts = QProcess::splitCommand(cmdline);
        if (parts.isEmpty()) continue;
        startClient(lang, parts.first(), parts.mid(1));
    }
}

void LspManager::startClient(const QString& languageId, const QString& command, const QStringList& args)
{
    if (auto* existing = clientForLanguage(languageId)) {
        if (existing->isRunning()) return;
        existing->deleteLater();
        m_clients.remove(languageId);
    }

    auto* client = new LspClient(languageId, command, args, this);
    m_clients.insert(languageId, client);
    connect(client, &LspClient::diagnosticsReceived, this, &LspManager::onDiagnostics);
    connect(client, &LspClient::serverExited, this, [this, languageId]() { onServerExited(languageId); });
    client->start(m_workspaceRoot);
    CF_LOG_INFO(QStringLiteral("LSP: started %1 server (%2)").arg(languageId, command));
}

LspClient* LspManager::clientForLanguage(const QString& languageId) const
{
    return m_clients.value(languageId, nullptr);
}

void LspManager::ensureStarted(TextDocument* doc)
{
    if (!doc || doc->isUntitled()) return;
    const QString lang = doc->languageId();
    if (lang.isEmpty()) return;
    SettingsManager& s = SettingsManager::instance();
    if (!s.getBool(QStringLiteral("lsp.enabled"))) return;
    if (clientForLanguage(lang) && clientForLanguage(lang)->isRunning()) return;

    // Auto-start only for languages the user configured.
    const QStringList servers = s.get(QStringLiteral("lsp.servers")).toStringList();
    for (const QString& entry : servers) {
        if (entry.startsWith(lang + QLatin1Char('='))) {
            const QStringList parts = QProcess::splitCommand(entry.mid(lang.size() + 1).trimmed());
            if (!parts.isEmpty()) startClient(lang, parts.first(), parts.mid(1));
            return;
        }
    }
}

void LspManager::handleDocumentOpened(TextDocument* doc)
{
    if (!doc || doc->isUntitled()) return;
    ensureStarted(doc);

    LspClient* client = clientForLanguage(doc->languageId());
    if (!client) return;
    client->didOpen(doc->filePath(), doc->languageId(), doc->document()->toPlainText());

    // Debounced didChange on edits. Timer owned by the manager; the map
    // entry is removed when the document is destroyed.
    auto* timer = new QTimer(this);
    timer->setSingleShot(true);
    timer->setInterval(400);
    connect(timer, &QTimer::timeout, this, [this, doc]() {
        if (!m_changeTimers.contains(doc)) return;   // stale (doc closed)
        if (!doc || doc->isUntitled()) return;
        LspClient* c = clientForLanguage(doc->languageId());
        if (c && c->isRunning() && doc->document()->characterCount() < 2000000)
            c->didChange(doc->filePath(), doc->document()->toPlainText());
    });
    connect(doc->document(), &QTextDocument::contentsChange, timer, qOverload<>(&QTimer::start));
    connect(doc, &QObject::destroyed, this, [this, doc]() { m_changeTimers.remove(doc); });
    m_changeTimers.insert(doc, timer);
}

void LspManager::handleDocumentClosed(const QString& docId, const QString& path)
{
    Q_UNUSED(docId);
    if (path.isEmpty()) return;
    m_diagnostics.remove(path);
    emit diagnosticsUpdated(path);
    for (LspClient* client : std::as_const(m_clients))
        if (client->isRunning()) client->didClose(path);
}

void LspManager::onDiagnostics(const QString& path, const QVector<Diagnostic>& diags)
{
    m_diagnostics.insert(path, diags);
    emit diagnosticsUpdated(path);
}

void LspManager::onServerExited(const QString& languageId)
{
    // Restart once on unexpected exit; give the system a beat.
    Q_UNUSED(languageId);
}

void LspManager::hover(TextDocument* doc, int line, int col, std::function<void(const QString&)> cb)
{
    if (!doc) return;
    LspClient* client = clientForLanguage(doc->languageId());
    if (!client || !client->isRunning()) { cb(QString()); return; }
    const QPointer<TextDocument> guard(doc);
    const QString path = doc->filePath();
    client->requestHover(path, line, col,
                         [guard, cb](const QJsonValue& result, bool isError) {
            if (isError || !guard) { cb(QString()); return; }
            if (result.isObject()) {
                const QJsonObject hoverObj = result.toObject();
                QString html;
                const QJsonValue contents = hoverObj.value(QStringLiteral("contents"));
                if (contents.isObject()) {
                    const QJsonObject c = contents.toObject();
                    const QString value = c.value(QStringLiteral("value")).toString();
                    if (!value.isEmpty()) html = QStringLiteral("<pre>%1</pre>").arg(value.toHtmlEscaped());
                } else if (contents.isString()) {
                    html = QStringLiteral("<pre>%1</pre>").arg(contents.toString().toHtmlEscaped());
                } else if (contents.isArray()) {
                    QJsonArray arr = contents.toArray();
                    for (const QJsonValue& v : arr)
                        if (v.isObject()) {
                            const QString value = v.toObject().value(QStringLiteral("value")).toString();
                            if (!value.isEmpty()) { html = QStringLiteral("<pre>%1</pre>").arg(value.toHtmlEscaped()); break; }
                        }
                }
                cb(html);
                return;
            }
            cb(QString());
        });
}

void LspManager::definition(TextDocument* doc, int line, int col,
                            std::function<void(const QString&, int, int)> cb)
{
    if (!doc) return;
    LspClient* client = clientForLanguage(doc->languageId());
    if (!client || !client->isRunning()) { cb(QString(), -1, -1); return; }
    const QPointer<TextDocument> guard(doc);
    client->requestDefinition(doc->filePath(), line, col,
                              [guard, cb](const QJsonValue& result, bool isError) {
            if (isError || !guard) { cb(QString(), -1, -1); return; }
            QJsonObject loc;
            if (result.isObject()) loc = result.toObject();
            else if (result.isArray() && result.toArray().size() > 0)
                loc = result.toArray().first().toObject();
            if (loc.isEmpty()) { cb(QString(), -1, -1); return; }

            const QString uri = loc.value(QStringLiteral("uri")).toString(
                loc.value(QStringLiteral("targetUri")).toString());
            const QJsonObject range = loc.contains(QStringLiteral("range"))
                                          ? loc.value(QStringLiteral("range")).toObject()
                                          : loc.value(QStringLiteral("targetSelectionRange")).toObject();
            const QJsonObject start = range.value(QStringLiteral("start")).toObject();
            const QString path = lsp::pathFromUri(uri);
            if (path.isEmpty()) { cb(QString(), -1, -1); return; }
            cb(path, start.value(QStringLiteral("line")).toInt(), start.value(QStringLiteral("character")).toInt());
        });
}

bool LspManager::hasDiagnostics(const QString& path) const
{
    return m_diagnostics.contains(path);
}

QVector<Diagnostic> LspManager::diagnosticsFor(const QString& path) const
{
    return m_diagnostics.value(path);
}

}  // namespace cf
