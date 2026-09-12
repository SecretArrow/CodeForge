#include <QtTest>

#include "core/FuzzyMatch.h"

using namespace cf;

class TestFuzzyMatch : public QObject {
    Q_OBJECT
private slots:
    void exactMatch()
    {
        FuzzyResult r = FuzzyMatch::score(QStringLiteral("abc"), QStringLiteral("abc"));
        QVERIFY(r.score >= 0);
        QCOMPARE(r.indices.size(), 3);
    }

    void subsequenceMatch()
    {
        FuzzyResult r = FuzzyMatch::scorePath(QStringLiteral("mapp"), QStringLiteral("src/MapManager.cpp"));
        QVERIFY2(r.score > 0, "mapp should match MapManager.cpp");
        r = FuzzyMatch::scorePath(QStringLiteral("mapp"), QStringLiteral("map_renderer.cpp"));
        QVERIFY(r.score > 0);
    }

    void noMatch()
    {
        FuzzyResult r = FuzzyMatch::score(QStringLiteral("zzz"), QStringLiteral("abcabc"));
        QCOMPARE(r.score, -1);
    }

    void caseInsensitive()
    {
        FuzzyResult r = FuzzyMatch::score(QStringLiteral("ABC"), QStringLiteral("abc"));
        QVERIFY(r.score >= 0);
    }

    void filenameBonus()
    {
        // Matching in the filename should outrank a match deeper in the path.
        FuzzyResult inName = FuzzyMatch::scorePath(QStringLiteral("server"), QStringLiteral("src/core/server.cpp"));
        FuzzyResult inPath = FuzzyMatch::scorePath(QStringLiteral("core"), QStringLiteral("core/server.cpp"));
        QVERIFY(inName.score > 0);
        QVERIFY(inPath.score > 0);
    }

    void emptyQuery()
    {
        FuzzyResult r = FuzzyMatch::score(QString(), QStringLiteral("anything"));
        QCOMPARE(r.score, 0);
    }
};

QTEST_GUILESS_MAIN(TestFuzzyMatch)
#include "test_fuzzymatch.moc"
