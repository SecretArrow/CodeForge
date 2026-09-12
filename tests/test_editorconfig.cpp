// .editorconfig parser/matcher tests.
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include "filesystem/EditorConfig.h"

using namespace cf;

class TestEditorConfig : public QObject {
    Q_OBJECT
private slots:
    void sectionGlobMatching();
    void propsParsing();
    void resolveFromDisk();
    void tabIndentSizeMarker();
};

void TestEditorConfig::sectionGlobMatching()
{
    QVERIFY(EditorConfig::sectionMatches(QStringLiteral("*"), QStringLiteral("main.cpp")));
    QVERIFY(EditorConfig::sectionMatches(QStringLiteral("*.py"), QStringLiteral("script.py")));
    QVERIFY(!EditorConfig::sectionMatches(QStringLiteral("*.py"), QStringLiteral("main.cpp")));
    // ** crosses directories
    QVERIFY(EditorConfig::sectionMatches(QStringLiteral("**/test*.cpp"), QStringLiteral("src/core/test_core.cpp")));
    QVERIFY(EditorConfig::sectionMatches(QStringLiteral("src/**"), QStringLiteral("src/a/b.txt")));
    // {alt}
    QVERIFY(EditorConfig::sectionMatches(QStringLiteral("*.{h,cpp}"), QStringLiteral("a.h")));
    QVERIFY(EditorConfig::sectionMatches(QStringLiteral("*.{h,cpp}"), QStringLiteral("a.cpp")));
    QVERIFY(!EditorConfig::sectionMatches(QStringLiteral("*.{h,cpp}"), QStringLiteral("a.c")));
    // Patterns without slash match basename only.
    QVERIFY(EditorConfig::sectionMatches(QStringLiteral("Makefile"), QStringLiteral("sub/Makefile")));
    // Patterns with slash match the relative path.
    QVERIFY(EditorConfig::sectionMatches(QStringLiteral("docs/*.md"), QStringLiteral("docs/readme.md")));
    QVERIFY(!EditorConfig::sectionMatches(QStringLiteral("docs/*.md"), QStringLiteral("readme.md")));
}

void TestEditorConfig::propsParsing()
{
    const EditorConfigProps p = EditorConfig::parseProps(QStringList{
        QStringLiteral("indent_style = space"),
        QStringLiteral("indent_size = 2"),
        QStringLiteral("end_of_line = lf"),
        QStringLiteral("trim_trailing_whitespace = true"),
        QStringLiteral("insert_final_newline = false"),
        QStringLiteral("charset = utf8"),
    });
    QCOMPARE(p.indentStyle, QString("space"));
    QCOMPARE(p.indentSize, 2);
    QCOMPARE(p.endOfLine, QString("lf"));
    QVERIFY(p.trimTrailingSet && p.trimTrailing);
    QVERIFY(p.finalNewlineSet && !p.finalNewline);
    QCOMPARE(p.charset, QString("utf8"));
}

void TestEditorConfig::resolveFromDisk()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QFile cfg(dir.filePath(QStringLiteral(".editorconfig")));
    QVERIFY(cfg.open(QIODevice::WriteOnly | QIODevice::Text));
    cfg.write("root = true\n\n"
              "[*]\n"
              "indent_style = space\n"
              "indent_size = 4\n\n"
              "[*.py]\n"
              "indent_size = 2\n"
              "insert_final_newline = true\n");
    cfg.close();

    const QString cpp = dir.filePath(QStringLiteral("main.cpp"));
    const QString py = dir.filePath(QStringLiteral("tool.py"));

    const EditorConfigProps cp = EditorConfig::resolve(cpp, dir.path());
    QVERIFY(cp.valid);
    QCOMPARE(cp.indentStyle, QString("space"));
    QCOMPARE(cp.indentSize, 4);
    QVERIFY(!cp.finalNewlineSet);   // [*] has no final_newline

    const EditorConfigProps pp = EditorConfig::resolve(py, dir.path());
    QVERIFY(pp.valid);
    QCOMPARE(pp.indentSize, 2);     // [*.py] overrides [*]
    QVERIFY(pp.finalNewlineSet && pp.finalNewline);
}

void TestEditorConfig::tabIndentSizeMarker()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QFile cfg(dir.filePath(QStringLiteral(".editorconfig")));
    QVERIFY(cfg.open(QIODevice::WriteOnly | QIODevice::Text));
    cfg.write("root = true\n[*]\nindent_style = tab\nindent_size = tab\ntab_width = 8\n");
    cfg.close();

    const EditorConfigProps p = EditorConfig::resolve(dir.filePath(QStringLiteral("x.cpp")), dir.path());
    QCOMPARE(p.indentStyle, QString("tab"));
    QCOMPARE(p.indentSize, 8);   // indent_size=tab resolves to tab_width
    QCOMPARE(p.tabWidth, 8);
}

QTEST_GUILESS_MAIN(TestEditorConfig)
#include "test_editorconfig.moc"
