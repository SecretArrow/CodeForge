// Multi-cursor operation tests (pure cursor math, no widgets).
#include <QTextCursor>
#include <QTextDocument>
#include <QtTest/QtTest>

#include "editor/MultiCursorOps.h"

using namespace cf;

class TestMultiCursor : public QObject {
    Q_OBJECT
private slots:
    void insertAtMultipleCursors();
    void backspaceRemovesPerCursor();
    void newlineKeepsIndent();
    void nextOccurrenceSkipsTaken();
    void wordRangeAt();
    void normalizeDropsOverlaps();
};

void TestMultiCursor::insertAtMultipleCursors()
{
    QTextDocument doc("a_b_c");
    QTextCursor c1(&doc); c1.setPosition(0);
    QTextCursor c2(&doc); c2.setPosition(2);
    QTextCursor c3(&doc); c3.setPosition(4);
    QList<QTextCursor> cursors{c1, c2, c3};

    multicursor::insertText(cursors, QStringLiteral("X"));
    QCOMPARE(doc.toPlainText(), QString("Xa_Xb_Xc"));
    // Every cursor sits after its inserted X.
    for (const QTextCursor& c : cursors)
        QCOMPARE(doc.characterAt(c.position() - 1), QChar('X'));
}

void TestMultiCursor::backspaceRemovesPerCursor()
{
    QTextDocument doc("ab cd ef");
    QTextCursor c1(&doc); c1.setPosition(2);   // after "ab"
    QTextCursor c2(&doc); c2.setPosition(5);   // after "cd"
    QList<QTextCursor> cursors{c1, c2};

    multicursor::backspace(cursors, 4, true);
    QCOMPARE(doc.toPlainText(), QString("a c ef"));
}

void TestMultiCursor::newlineKeepsIndent()
{
    QTextDocument doc;
    doc.setPlainText("    hello");
    QTextCursor c(&doc);
    c.setPosition(9);   // end of line
    QList<QTextCursor> cursors{c};

    multicursor::newline(cursors);
    QCOMPARE(doc.toPlainText(), QString("    hello\n    "));
    QCOMPARE(cursors.first().blockNumber(), 1);
    QCOMPARE(cursors.first().positionInBlock(), 4);
}

void TestMultiCursor::nextOccurrenceSkipsTaken()
{
    QTextDocument doc;
    doc.setPlainText("foo bar foo bar foo");
    QList<QPair<int, int>> taken;
    // First "foo" = [0,3). Next match from 3 must be at 8.
    QCOMPARE(multicursor::nextOccurrence(&doc, QStringLiteral("foo"), 3, false, taken), 8);
    taken.append({8, 11});
    // Skip [8,11) -> next is 16.
    QCOMPARE(multicursor::nextOccurrence(&doc, QStringLiteral("foo"), 3, false, taken), 16);
    // Whole word: needle "ba" should not match inside "bar".
    QCOMPARE(multicursor::nextOccurrence(&doc, QStringLiteral("ba"), 0, true, {}), -1);
    QCOMPARE(multicursor::nextOccurrence(&doc, QStringLiteral("ba"), 0, false, {}), 4);
}

void TestMultiCursor::wordRangeAt()
{
    QTextDocument doc;
    doc.setPlainText("int value = 10;");
    const auto r1 = multicursor::wordRangeAt(&doc, 5);   // inside "value"
    QCOMPARE(doc.toPlainText().mid(r1.first, r1.second - r1.first), QString("value"));
    const auto r2 = multicursor::wordRangeAt(&doc, 10);  // on space
    QCOMPARE(r2.first, -1);
}

void TestMultiCursor::normalizeDropsOverlaps()
{
    QTextDocument doc("abcdef");
    QTextCursor c1(&doc); c1.setPosition(0); c1.setPosition(3, QTextCursor::KeepAnchor);   // abc
    QTextCursor c2(&doc); c2.setPosition(2); c2.setPosition(5, QTextCursor::KeepAnchor);   // overlaps
    QTextCursor c3(&doc); c3.setPosition(5);                                               // cursor only
    QList<QTextCursor> cursors{c2, c1, c3};
    multicursor::normalize(cursors);
    QCOMPARE(cursors.size(), 2);
}

QTEST_GUILESS_MAIN(TestMultiCursor)
#include "test_multicursor.moc"
