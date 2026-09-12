#pragma once
// LspClient: real Language Server Protocol client over stdio (QProcess).
// Handles initialize handshake, document synchronization (full sync),
// publishDiagnostics, and request/response correlation for hover and
// definition. Notifications sent before initialization completes are queued.
#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QProcess>
#include <QVector>

#include "lsp/LanguageService.h"

class QStringList;

namespace cf {

class LspClient : public QObject {
    Q_OBJECT
public:
    LspClient(const QString& languageId, const QString& command, const QStringList& args,
              QObject* parent = nullptr);
    ~LspClient() override;

    void start(const QString& workspaceRoot);
    void stop();                                  // graceful shutdown + kill
    bool isRunning() const;
    bool isInitialized() const { return m_initialized; }

    // ---- document sync ----
    void didOpen(const QString& path, const QString& languageId, const QString& text);
    void didChange(const QString& path, const QString& text);
    void didClose(const QString& path);

    // ---- requests ----
    using ResponseCallback = std::function<void(const QJsonValue& result, bool isError)>;
    void requestHover(const QString& path, int line, int col, ResponseCallback cb);
    void requestDefinition(const QString& path, int line, int col, ResponseCallback cb);

signals:
    void diagnosticsReceived(const QString& path, const QVector<cf::Diagnostic>& diags);
    void serverExited();

private slots:
    void onReadyRead();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    void send(const QJsonObject& message);
    void sendQueued();
    void handleServerMessage(const QJsonObject& message);
    static QJsonObject textDocumentId(const QString& path);

    QString m_languageId;
    QString m_command;
    QStringList m_args;
    QProcess m_process;
    QByteArray m_buffer;
    QHash<QString, int> m_versions;

    int m_nextId = 1;
    bool m_initialized = false;
    bool m_stopping = false;
    QHash<int, ResponseCallback> m_pending;
    QList<QByteArray> m_queuedNotifications;   // held until "initialized"
};

}  // namespace cf
