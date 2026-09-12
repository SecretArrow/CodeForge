#include "lsp/LspClient.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QRegularExpression>
#include <QTimer>

#include "core/Logger.h"
#include "lsp/LspProtocol.h"

namespace cf {

LspClient::LspClient(const QString& languageId, const QString& command, const QStringList& args,
                     QObject* parent)
    : QObject(parent), m_languageId(languageId), m_command(command), m_args(args)
{
    m_process.setProgram(command);
    m_process.setArguments(args);
    connect(&m_process, &QProcess::readyReadStandardOutput, this, &LspClient::onReadyRead);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        CF_LOG_WARNING(QStringLiteral("LSP[%1]: process error: %2").arg(m_languageId, m_process.errorString()));
    });
    connect(&m_process, &QProcess::finished, this, &LspClient::onProcessFinished);
}

LspClient::~LspClient()
{
    stop();
}

void LspClient::start(const QString& workspaceRoot)
{
    if (m_process.state() != QProcess::NotRunning) return;
    m_stopping = false;
    m_buffer.clear();
    m_initialized = false;
    m_queuedNotifications.clear();

    m_process.setWorkingDirectory(workspaceRoot);
    m_process.start();
    if (!m_process.waitForStarted(5000)) {
        CF_LOG_ERROR(QStringLiteral("LSP[%1]: failed to start '%2': %3")
                         .arg(m_languageId, m_command, m_process.errorString()));
        return;
    }

    QJsonObject rootUri;
    rootUri.insert(QStringLiteral("rootUri"),
                   workspaceRoot.isEmpty() ? QJsonValue(QString())
                                           : QJsonValue(lsp::uriFromPath(workspaceRoot)));

    QJsonObject capabilities;   // we announce nothing fancy; full-sync text documents
    QJsonObject textDocument;
    textDocument.insert(QStringLiteral("sync"), 1);   // FullTextDocumentSync = 1
    capabilities.insert(QStringLiteral("textDocument"), textDocument);

    QJsonObject params;
    params.insert(QStringLiteral("processId"), QCoreApplication::applicationPid());
    params.insert(QStringLiteral("rootUri"), rootUri.value(QStringLiteral("rootUri")));
    params.insert(QStringLiteral("capabilities"), capabilities);
    params.insert(QStringLiteral("workspaceFolders"), QJsonArray());

    const int id = m_nextId++;
    QJsonObject init = lsp::buildRequest(id, QStringLiteral("initialize"), params);
    m_pending.insert(id, [this](const QJsonValue& result, bool isError) {
        Q_UNUSED(result);
        m_initialized = true;
        if (isError) {
            CF_LOG_ERROR(QStringLiteral("LSP[%1]: initialize failed").arg(m_languageId));
            return;
        }
        send(lsp::buildNotification(QStringLiteral("initialized"), QJsonObject()));
        sendQueued();
        CF_LOG_INFO(QStringLiteral("LSP[%1]: server ready").arg(m_languageId));
    });
    send(init);
}

void LspClient::stop()
{
    if (m_process.state() == QProcess::NotRunning) return;
    m_stopping = true;
    m_pending.clear();
    if (m_initialized) {
        send(lsp::buildRequest(m_nextId++, QStringLiteral("shutdown"), QJsonObject()));
        send(lsp::buildNotification(QStringLiteral("exit"), QJsonObject()));
        m_process.waitForFinished(1000);
    }
    if (m_process.state() != QProcess::NotRunning) {
        m_process.terminate();
        if (!m_process.waitForFinished(800)) m_process.kill();
    }
}

bool LspClient::isRunning() const
{
    return m_process.state() != QProcess::NotRunning;
}

void LspClient::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(exitCode);
    m_initialized = false;
    if (!m_stopping)
        CF_LOG_WARNING(QStringLiteral("LSP[%1]: server exited (%2)")
                           .arg(m_languageId, status == QProcess::CrashExit ? QStringLiteral("crash")
                                                                            : QStringLiteral("normal")));
    emit serverExited();
}

void LspClient::send(const QJsonObject& message)
{
    if (m_process.state() != QProcess::Running) return;
    m_process.write(lsp::makeFrame(message));
}

void LspClient::sendQueued()
{
    for (const QByteArray& frame : std::as_const(m_queuedNotifications)) m_process.write(frame);
    m_queuedNotifications.clear();
}

void LspClient::didOpen(const QString& path, const QString& languageId, const QString& text)
{
    QJsonObject params;
    QJsonObject doc;
    doc.insert(QStringLiteral("uri"), lsp::uriFromPath(path));
    doc.insert(QStringLiteral("languageId"), languageId);
    doc.insert(QStringLiteral("version"), 1);
    doc.insert(QStringLiteral("text"), text);
    params.insert(QStringLiteral("textDocument"), doc);
    const QByteArray frame = lsp::makeFrame(lsp::buildNotification(QStringLiteral("textDocument/didOpen"), params));
    if (m_initialized) send(lsp::buildNotification(QStringLiteral("textDocument/didOpen"), params));
    else m_queuedNotifications.append(frame);
}

void LspClient::didChange(const QString& path, const QString& text)
{
    QJsonObject params;
    QJsonObject doc = lsp::textDocumentId(path);
    doc.insert(QStringLiteral("version"), ++m_versions[path]);
    QJsonArray changes;
    QJsonObject change;
    change.insert(QStringLiteral("text"), text);
    changes.append(change);
    params.insert(QStringLiteral("textDocument"), doc);
    params.insert(QStringLiteral("contentChanges"), changes);
    if (m_initialized) send(lsp::buildNotification(QStringLiteral("textDocument/didChange"), params));
    else m_queuedNotifications.append(lsp::makeFrame(lsp::buildNotification(QStringLiteral("textDocument/didChange"), params)));
}

void LspClient::didClose(const QString& path)
{
    QJsonObject params;
    params.insert(QStringLiteral("textDocument"), lsp::textDocumentId(path));
    if (m_initialized) send(lsp::buildNotification(QStringLiteral("textDocument/didClose"), params));
    else m_queuedNotifications.append(lsp::makeFrame(lsp::buildNotification(QStringLiteral("textDocument/didClose"), params)));
}

void LspClient::requestHover(const QString& path, int line, int col, ResponseCallback cb)
{
    if (!m_initialized) { cb(QJsonValue(), false); return; }
    QJsonObject params;
    params.insert(QStringLiteral("textDocument"), lsp::textDocumentId(path));
    params.insert(QStringLiteral("position"), lsp::positionJson(line, col));
    const int id = m_nextId++;
    m_pending.insert(id, std::move(cb));
    send(lsp::buildRequest(id, QStringLiteral("textDocument/hover"), params));
}

void LspClient::requestDefinition(const QString& path, int line, int col, ResponseCallback cb)
{
    if (!m_initialized) { cb(QJsonValue(), false); return; }
    QJsonObject params;
    params.insert(QStringLiteral("textDocument"), lsp::textDocumentId(path));
    params.insert(QStringLiteral("position"), lsp::positionJson(line, col));
    const int id = m_nextId++;
    m_pending.insert(id, std::move(cb));
    send(lsp::buildRequest(id, QStringLiteral("textDocument/definition"), params));
}

QJsonObject LspClient::textDocumentId(const QString& path)
{
    QJsonObject doc;
    doc.insert(QStringLiteral("uri"), lsp::uriFromPath(path));
    return doc;
}

void LspClient::onReadyRead()
{
    m_buffer.append(m_process.readAllStandardOutput());
    QByteArray frame;
    while (lsp::extractFrame(m_buffer, &frame)) {
        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(frame, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            CF_LOG_WARNING(QStringLiteral("LSP[%1]: bad JSON frame (%2)").arg(m_languageId, err.errorString()));
            continue;
        }
        handleServerMessage(doc.object());
    }
}

void LspClient::handleServerMessage(const QJsonObject& msg)
{
    if (msg.contains(QStringLiteral("id")) && (msg.contains(QStringLiteral("result")) || msg.contains(QStringLiteral("error")))) {
        const int id = msg.value(QStringLiteral("id")).toInt(-1);
        auto it = m_pending.find(id);
        if (it != m_pending.end()) {
            const ResponseCallback cb = it.value();
            m_pending.erase(it);
            if (msg.contains(QStringLiteral("error")))
                cb(QJsonValue(), true);
            else
                cb(msg.value(QStringLiteral("result")), false);
        }
        return;
    }

    const QString method = msg.value(QStringLiteral("method")).toString();
    const QJsonObject params = msg.value(QStringLiteral("params")).toObject();
    if (method == QLatin1String("textDocument/publishDiagnostics")) {
        const QString uri = params.value(QStringLiteral("uri")).toString();
        const QString path = lsp::pathFromUri(uri);
        QVector<Diagnostic> diags;
        const QJsonArray items = params.value(QStringLiteral("diagnostics")).toArray();
        for (const QJsonValue& v : items) {
            const QJsonObject d = v.toObject();
            Diagnostic diag;
            const int sev = d.value(QStringLiteral("severity")).toInt(2);
            switch (sev) {
            case 1: diag.severity = Diagnostic::Error; break;
            case 2: diag.severity = Diagnostic::Warning; break;
            case 3: diag.severity = Diagnostic::Info; break;
            default: diag.severity = Diagnostic::Hint; break;
            }
            const QJsonObject range = d.value(QStringLiteral("range")).toObject();
            const QJsonObject start = range.value(QStringLiteral("start")).toObject();
            const QJsonObject end = range.value(QStringLiteral("end")).toObject();
            diag.line = start.value(QStringLiteral("line")).toInt();
            diag.column = start.value(QStringLiteral("character")).toInt();
            diag.length = end.value(QStringLiteral("character")).toInt() - diag.column;
            diag.message = d.value(QStringLiteral("message")).toString();
            diag.source = d.value(QStringLiteral("source")).toString(m_languageId);
            diags.append(diag);
        }
        emit diagnosticsReceived(path, diags);
    }
    // window/logMessage, window/showMessage etc. are ignored (offline, no UI noise).
}

}  // namespace cf
