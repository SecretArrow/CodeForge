#pragma once
// LSP JSON-RPC 2.0 over stdio: framing primitives (pure, unit-testable) and
// message builders. The transport/client lives in LspClient.
#include <QByteArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

namespace cf {
namespace lsp {

// Extracts one complete Content-Length framed message from `buffer`.
// On success: the frame body is removed from `buffer`, written to `content`,
// and true is returned. Returns false when the buffer holds no full frame.
// Malformed headers are skipped so the stream can recover.
bool extractFrame(QByteArray& buffer, QByteArray* content);

// Wraps a JSON object into a Content-Length framed byte sequence.
QByteArray makeFrame(const QJsonObject& message);

QJsonObject buildRequest(int id, const QString& method, const QJsonObject& params);
QJsonObject buildNotification(const QString& method, const QJsonObject& params);

// file:/// URI conversion (percent-encoded, LSP compliant).
QString uriFromPath(const QString& path);
QString pathFromUri(const QString& uri);

QJsonObject positionJson(int line, int character);

}  // namespace lsp
}  // namespace cf
