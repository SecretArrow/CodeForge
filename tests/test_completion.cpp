// Completion + snippet expansion tests (pure helpers; no real widgets).
#include <QTextCursor>
#include <QTextDocument>
#include <QtTest>

#include "autocomplete/CompletionEngine.h"

using namespace cf;

class TestCompletion : public QObject {
    Q_OBJECT
private slots:
    void wordBeforeCursorBasics()
    {
        QTextDocument doc(QStringLiteral("hello wor_ld again"));
        QTextCursor c(&doc);
        c.setPosition(9);   // after "wor" (position 6..9)
        QCOMPARE(CompletionEngine::wordBeforeCursor(c), QStringLiteral("wor"));
        c.setPosition(13);  // after "ld"
        QCOMPARE(CompletionEngine::wordBeforeCursor(c), QStringLiteral("wor_ld"));
        c.setPosition(5);   // after "hello"
        QCOMPARE(CompletionEngine::wordBeforeCursor(c), QStringLiteral("hello"));
    }

    void filterRanksPrefixFirst()
    {
        QVector<CompletionItem> items;
        items.append({ QStringLiteral("print"), QString(), QString(), 0, false, QString() });
        items.append({ QStringLiteral("println"), QString(), QString(), 0, false, QString() });
        items.append({ QStringLiteral("sprintf"), QString(), QString(), 0, false, QString() });
        items.append({ QStringLiteral("Println"), QString(), QString(), 0, false, QString() });
        const auto out = CompletionEngine::filterRank(items, QStringLiteral("print"));
        // startsWith (case-sensitive exact prefix) ranks before contains only;
        // "sprintf" contains but does not start with "print" -> excluded
        for (const auto& it : out)
            QVERIFY2(it.label.startsWith(QStringLiteral("print"), Qt::CaseInsensitive),
                     qPrintable(it.label));
        QVERIFY(!out.isEmpty());
    }

    void filterEmptyPrefixKeepsAll()
    {
        QVector<CompletionItem> items;
        items.append({ QStringLiteral("a"), QString(), QString(), 0, false, QString() });
        items.append({ QStringLiteral("b"), QString(), QString(), 0, false, QString() });
        QCOMPARE(CompletionEngine::filterRank(items, QString()).size(), 2);
    }
};

QTEST_GUILESS_MAIN(TestCompletion)
#include "test_completion.moc"
