#include <QtTest>

#include "core/Encoding.h"

using namespace cf;

class TestEncoding : public QObject {
    Q_OBJECT
private slots:
    void detectUtf8()
    {
        const QByteArray data("Hello, world! \xE2\x9C\x93");
        const enc::Info e = enc::detect(data);
        QCOMPARE(e.id, enc::Id::Utf8);
        bool ok = false;
        const QString text = enc::decode(data, e, &ok);
        QVERIFY(ok);
        QCOMPARE(text, QStringLiteral("Hello, world! ✓"));
    }

    void detectUtf8Bom()
    {
        const QByteArray data("\xEF\xBB\xBF" "hello");
        const enc::Info e = enc::detect(data);
        QCOMPARE(e.id, enc::Id::Utf8Bom);
        const QString text = enc::decode(data, e);
        QCOMPARE(text, QStringLiteral("hello"));
    }

    void detectUtf16LE()
    {
        const QByteArray data("\xFF\xFE" "h\0" "i\0", 6);
        const enc::Info e = enc::detect(data);
        QCOMPARE(e.id, enc::Id::Utf16LE);
        const QString text = enc::decode(data, e);
        QCOMPARE(text, QStringLiteral("hi"));
    }

    void roundTripUtf16()
    {
        enc::Info e; e.id = enc::Id::Utf16LE;
        const QString original = QStringLiteral("Héllo wörld ✓");
        bool ok = false;
        const QByteArray bytes = enc::encode(original, e, &ok);
        QVERIFY(ok);
        const QString back = enc::decode(bytes, e, &ok);
        QVERIFY(ok);
        QCOMPARE(back, original);
    }

    void invalidUtf8FallsBack()
    {
        const QByteArray data("caf\xE9 latin1");
        const enc::Info e = enc::detect(data);
        QCOMPARE(e.id, enc::Id::Windows1252);
    }

    void detectLineEndings()
    {
        QCOMPARE(enc::detectLineEndings(QStringLiteral("a\r\nb\r\nc")), LineEndings::Crlf);
        QCOMPARE(enc::detectLineEndings(QStringLiteral("a\nb\nc")), LineEndings::Lf);
        QCOMPARE(enc::detectLineEndings(QStringLiteral("a\rb\rc")), LineEndings::Cr);
        QCOMPARE(enc::detectLineEndings(QStringLiteral("abc")), LineEndings::Lf);
    }
};

QTEST_GUILESS_MAIN(TestEncoding)
#include "test_encoding.moc"
