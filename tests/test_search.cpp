#include <QtTest>

#include <QTextDocument>

#include "search/SearchEngine.h"

using namespace cf;

class TestSearch : public QObject {
    Q_OBJECT
private slots:
    void literalSearch()
    {
        SearchQuery q;
        q.text = QStringLiteral("TODO");
        q.caseSensitive = true;
        QVector<SearchHit> hits;
        const int n = search_detail::findMatchesInText(
            QStringLiteral("// TODO fix\nint x; // todo lower\n// TODO again"), q, &hits, 100);
        QCOMPARE(n, 2);
        QCOMPARE(hits.at(0).line, 0);
        QCOMPARE(hits.at(1).line, 2);
    }

    void caseInsensitiveSearch()
    {
        SearchQuery q;
        q.text = QStringLiteral("todo");
        q.caseSensitive = false;
        const int n = search_detail::findMatchesInText(
            QStringLiteral("TODO\nToDO\ntodo"), q, nullptr, 100);
        QCOMPARE(n, 3);
    }

    void regexSearch()
    {
        SearchQuery q;
        q.text = QStringLiteral("\\d{3}-\\d{4}");
        q.isRegex = true;
        QVector<SearchHit> hits;
        const int n = search_detail::findMatchesInText(
            QStringLiteral("call 555-1234 or 555-5678"), q, &hits, 100);
        QCOMPARE(n, 2);
        QCOMPARE(hits.at(0).colStart, 5);
    }

    void wholeWordSearch()
    {
        SearchQuery q;
        q.text = QStringLiteral("int");
        q.wholeWord = true;
        const int n = search_detail::findMatchesInText(QStringLiteral("int x; print integer"), q, nullptr, 100);
        QCOMPARE(n, 1);
    }

    void globMatching()
    {
        QVERIFY(search_detail::globMatches(QStringLiteral("src/main.cpp"), { QStringLiteral("src/**") }));
        QVERIFY(search_detail::globMatches(QStringLiteral("build/x/y.o"), { QStringLiteral("build/**") }));
        QVERIFY(!search_detail::globMatches(QStringLiteral("src/main.cpp"), { QStringLiteral("build/**") }));
    }
};

QTEST_GUILESS_MAIN(TestSearch)
#include "test_search.moc"
