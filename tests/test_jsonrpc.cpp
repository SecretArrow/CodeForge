// LSP JSON-RPC framing tests.
#include <QtTest/QtTest>

#include "lsp/LspProtocol.h"

using namespace cf;
using namespace cf::lsp;

class TestJsonRpc : public QObject {
    Q_OBJECT
private slots:
    void singleFrame();
    void twoFramesInOneChunk();
    void frameSplitAcrossChunks();
    void malformedFrameSkipped();
    void makeFrameRoundtrip();
    void uriRoundtrip();
};

void TestJsonRpc::singleFrame()
{
    QByteArray buf = "Content-Length: 18\r\n\r\n{\"method\":\"x\"}\r\nextra";
    QByteArray out;
    QVERIFY(extractFrame(buf, &out));
    QCOMPARE(out, QByteArray("{\"method\":\"x\"}"));
    QCOMPARE(buf, QByteArray("\r\nextra"));
}

void TestJsonRpc::twoFramesInOneChunk()
{
    const QByteArray f1 = makeFrame(buildNotification(QStringLiteral("a"), {}));
    const QByteArray f2 = makeFrame(buildNotification(QStringLiteral("b"), {}));
    QByteArray buf = f1 + f2;
    QByteArray out;
    QVERIFY(extractFrame(buf, &out));
    QVERIFY(!out.isEmpty());
    QVERIFY(extractFrame(buf, &out));
    QVERIFY(!out.isEmpty());
    QVERIFY(!extractFrame(buf, &out));   // empty now
}

void TestJsonRpc::frameSplitAcrossChunks()
{
    const QByteArray frame = makeFrame(buildRequest(1, QStringLiteral("initialize"), {}));
    QByteArray buf = frame.left(frame.size() / 2);
    QByteArray out;
    QVERIFY(!extractFrame(buf, &out));          // incomplete: waits
    buf.append(frame.mid(frame.size() / 2));    // complete now
    QVERIFY(extractFrame(buf, &out));
    QVERIFY(out.contains("\"initialize\""));
}

void TestJsonRpc::malformedFrameSkipped()
{
    QByteArray buf = "Content-Type: nonsense\r\n\r\nXXX"
                     "Content-Length: 2\r\n\r\n{}";
    QByteArray out;
    QVERIFY(extractFrame(buf, &out));   // skips the malformed frame
    QCOMPARE(out, QByteArray("{}"));
}

void TestJsonRpc::makeFrameRoundtrip()
{
    QJsonObject msg = buildRequest(42, QStringLiteral("textDocument/hover"), {});
    const QByteArray frame = makeFrame(msg);
    QVERIFY(frame.startsWith("Content-Length:"));
    QByteArray buf = frame;
    QByteArray out;
    QVERIFY(extractFrame(buf, &out));
    const QJsonDocument doc = QJsonDocument::fromJson(out);
    QVERIFY(doc.isObject());
    QCOMPARE(doc.object().value("id").toInt(), 42);
    QCOMPARE(doc.object().value("method").toString(), QString("textDocument/hover"));
}

void TestJsonRpc::uriRoundtrip()
{
    const QString path = QStringLiteral("/home/user/my project/file.cpp");
    const QString uri = uriFromPath(path);
    QVERIFY(uri.startsWith(QStringLiteral("file:///")));
    QCOMPARE(pathFromUri(uri), path);
}

QTEST_GUILESS_MAIN(TestJsonRpc)
#include "test_jsonrpc.moc"
