#include "lsp/LspProtocol.h"

#include <QDir>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QUrl>

namespace cf {
namespace lsp {

bool extractFrame(QByteArray& buffer, QByteArray* content)
{
    forever {
        const int headerEnd = buffer.indexOf("\r\n\r\n");
        if (headerEnd < 0) {
            // Tolerate bare \n\n framing from non-conformant servers.
            const int bare = buffer.indexOf("\n\n");
            if (bare < 0) return false;
            const QByteArray headers = buffer.left(bare);
            QRegularExpressionMatch m =
                QRegularExpression(QStringLiteral("content-length:\\s*(\\d+)"),
                                   QRegularExpression::CaseInsensitiveOption)
                    .match(QString::fromLatin1(headers));
            if (!m.hasMatch()) {
                buffer.remove(0, bare + 2);
                continue;
            }
            const int length = m.captured(1).toInt();
            const int bodyStart = bare + 2;
            if (buffer.size() - bodyStart < length) return false;
            if (content) *content = buffer.mid(bodyStart, length);
            buffer.remove(0, bodyStart + length);
            return true;
        }

        const QByteArray headers = buffer.left(headerEnd);
        QRegularExpressionMatch m =
            QRegularExpression(QStringLiteral("content-length:\\s*(\\d+)"),
                               QRegularExpression::CaseInsensitiveOption)
                .match(QString::fromLatin1(headers));
        if (!m.hasMatch()) {
            buffer.remove(0, headerEnd + 4);
            continue;   // skip malformed frame
        }
        const int length = m.captured(1).toInt();
        const int bodyStart = headerEnd + 4;
        if (buffer.size() - bodyStart < length) return false;   // wait for more bytes
        if (content) *content = buffer.mid(bodyStart, length);
        buffer.remove(0, bodyStart + length);
        return true;
    }
}

QByteArray makeFrame(const QJsonObject& message)
{
    const QByteArray body = QJsonDocument(message).toJson(QJsonDocument::Compact);
    return QStringLiteral("Content-Length: %1\r\n\r\n").arg(body.size()).toLatin1() + body;
}

QJsonObject buildRequest(int id, const QString& method, const QJsonObject& params)
{
    QJsonObject msg;
    msg.insert(QStringLiteral("jsonrpc"), QStringLiteral("2.0"));
    msg.insert(QStringLiteral("id"), id);
    msg.insert(QStringLiteral("method"), method);
    msg.insert(QStringLiteral("params"), params);
    return msg;
}

QJsonObject buildNotification(const QString& method, const QJsonObject& params)
{
    QJsonObject msg;
    msg.insert(QStringLiteral("jsonrpc"), QStringLiteral("2.0"));
    msg.insert(QStringLiteral("method"), method);
    msg.insert(QStringLiteral("params"), params);
    return msg;
}

QString uriFromPath(const QString& path)
{
    return QUrl::fromLocalFile(QDir::fromNativeSeparators(path)).toString();
}

QString pathFromUri(const QString& uri)
{
    const QUrl url(uri);
    if (url.isLocalFile()) return url.toLocalFile();
    // Some servers send file:///c%3A/... or unencoded drive letters.
    if (url.scheme() == QLatin1String("file")) return url.path();
    return uri;
}

QJsonObject positionJson(int line, int character)
{
    QJsonObject pos;
    pos.insert(QStringLiteral("line"), qMax(0, line));
    pos.insert(QStringLiteral("character"), qMax(0, character));
    return pos;
}

}  // namespace lsp
}  // namespace cf
