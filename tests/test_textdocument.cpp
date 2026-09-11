#include <QtTest>

#include <QPlainTextDocumentLayout>
#include <QTemporaryDir>

#include "core/TextDocument.h"

using namespace cf;

class TestTextDocument : public QObject {
    Q_OBJECT
private slots:
    void loadSaveRoundTrip()
    {
        QTemporaryDir dir;
        const QString path = dir.path() + QStringLiteral("/sample.cpp");
        const QString content = QStringLiteral("int main() {\r\n    return 0;\r\n}\r\n");
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(content.toUtf8());
        f.close();

        TextDocument doc;
        doc.setFilePath(path);
        QString err;
        QVERIFY(doc.load(&err));

        // CRLF detected and preserved for saving.
        QCOMPARE(doc.lineEndings(), LineEndings::Crlf);
        QCOMPARE(doc.encoding().label(), QStringLiteral("UTF-8"));
        QVERIFY(!doc.isDirty());

        // Make a change and save.
        QTextCursor c(doc.document());
        c.movePosition(QTextCursor::End);
        c.insertText(QStringLiteral(" // edited"));
        QVERIFY(doc.isDirty());
        QVERIFY(doc.save(&err));
        QVERIFY(!doc.isDirty());

        // Verify saved bytes keep CRLF endings.
        QFile saved(path);
        QVERIFY(saved.open(QIODevice::ReadOnly));
        const QByteArray bytes = saved.readAll();
        QVERIFY(bytes.contains("\r\n"));
        QVERIFY(bytes.contains("// edited"));
    }

    void externalChangeDetection()
    {
        QTemporaryDir dir;
        const QString path = dir.path() + QStringLiteral("/watched.txt");
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("v1");
        f.close();

        TextDocument doc;
        doc.setFilePath(path);
        QString err;
        QVERIFY(doc.load(&err));
        doc.snapshotDiskState();
        QVERIFY(!doc.hasExternalChange());

        // Modify on disk with a different mtime.
        QFile g(path);
        QVERIFY(g.open(QIODevice::WriteOnly));
        g.write("v2 - changed");
        g.close();
        QVERIFY(doc.hasExternalChange());
    }

    void untitledIdentity()
    {
        TextDocument doc;
        QVERIFY(doc.isUntitled());
        QVERIFY(doc.docId().startsWith(QStringLiteral("untitled:")));
        QCOMPARE(doc.displayName(), QStringLiteral("Untitled-") + doc.docId().mid(9));
        QVERIFY(!doc.isDirty());
    }

    void readOnlyBlocksDirty()
    {
        TextDocument doc;
        doc.setReadOnly(true);
        QVERIFY(!doc.isDirty());
    }
};

QTEST_MAIN(TestTextDocument)
#include "test_textdocument.moc"
