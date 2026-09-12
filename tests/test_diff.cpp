// DiffEngine tests (pure logic).
#include <QtTest>

#include "diff/DiffEngine.h"

using namespace cf;

class TestDiff : public QObject {
    Q_OBJECT
private slots:
    void splitNormalizesEol()
    {
        QStringList l = DiffEngine::splitLines(QStringLiteral("a\r\nb\rc\n"));
        QCOMPARE(l.size(), 3);
        QCOMPARE(l.at(0), QStringLiteral("a"));
        QCOMPARE(l.at(1), QStringLiteral("b"));
        QCOMPARE(l.at(2), QStringLiteral("c"));
    }

    void identicalFilesProduceEqualRows()
    {
        DiffEngine e;
        e.compute(QStringLiteral("one\ntwo\n"), QStringLiteral("one\ntwo\n"));
        QCOMPARE(e.hunks().size(), 0);
        QCOMPARE(e.rows().size(), 3);   // 2 lines + trailing empty line
    }

    void insertionDetected()
    {
        const auto rows = DiffEngine::diffLines({ "a", "c" }, { "a", "b", "c" });
        int adds = 0, equals = 0;
        for (const auto& r : rows) {
            if (r.type == DiffRow::Type::Add)
                ++adds;
            else if (r.type == DiffRow::Type::Equal)
                ++equals;
        }
        QCOMPARE(adds, 1);
        QCOMPARE(equals, 2);
    }

    void deletionDetected()
    {
        const auto rows = DiffEngine::diffLines({ "a", "b", "c" }, { "a", "c" });
        int dels = 0;
        for (const auto& r : rows)
            if (r.type == DiffRow::Type::Delete)
                ++dels;
        QCOMPARE(dels, 1);
    }

    void replacementProducesBothSides()
    {
        DiffEngine e;
        e.compute(QStringLiteral("alpha\nbeta\ngamma\n"), QStringLiteral("alpha\nBETA\ngamma\n"));
        QCOMPARE(e.hunks().size(), 1);
        const DiffHunk h = e.hunks().first();
        QCOMPARE(h.aStart, 1);
        QCOMPARE(h.bStart, 1);
        QCOMPARE(h.rowCount, 2);   // one Delete row + one Add row
    }

    void applyMergeRightRemovesAddedLines()
    {
        // simulate what DiffViewer does when copying A -> B over the hunk
        DiffEngine e;
        e.compute(QStringLiteral("keep\nold\n"), QStringLiteral("keep\nnew1\nnew2\n"));
        QCOMPARE(e.hunks().size(), 1);
        QStringList tgt = DiffEngine::splitLines(e.rows().isEmpty() ? QString() : QStringLiteral("keep\nnew1\nnew2\n"));
        // B lines to remove: Add rows (2), replacement: Delete rows ("old")
        int start = -1, removeCount = 0;
        QStringList replacement;
        for (const DiffRow& r : e.rows()) {
            if (r.type == DiffRow::Type::Delete)
                replacement.append(r.left);
            else if (r.type == DiffRow::Type::Add) {
                if (start < 0)
                    start = r.bLine;
                ++removeCount;
            }
        }
        QCOMPARE(start, 1);
        QCOMPARE(removeCount, 2);
        for (int i = 0; i < removeCount; ++i)
            tgt.removeAt(start);
        for (int i = 0; i < replacement.size(); ++i)
            tgt.insert(start + i, replacement.at(i));
        QCOMPARE(tgt.join(QLatin1Char('\n')), QStringLiteral("keep\nold"));
    }

    void largeInputFallbackStillCoversAllLines()
    {
        // 5k x 5k lines: exceeds the DP budget -> fallback path must still
        // account for every line.
        QStringList a, b;
        for (int i = 0; i < 5000; ++i) {
            a.append(QString::number(i));
            b.append(QString::number(i));
        }
        a.append(QStringLiteral("only-in-a"));
        b.append(QStringLiteral("only-in-b"));
        DiffEngine e;
        e.compute(a.join(QLatin1Char('\n')), b.join(QLatin1Char('\n')));
        QVERIFY(e.usedFallback());
        int dels = 0, adds = 0;
        for (const auto& r : e.rows()) {
            if (r.type == DiffRow::Type::Delete) ++dels;
            if (r.type == DiffRow::Type::Add) ++adds;
        }
        QCOMPARE(dels, 1);
        QCOMPARE(adds, 1);
    }
};

QTEST_GUILESS_MAIN(TestDiff)
#include "test_diff.moc"
