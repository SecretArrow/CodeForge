// Markdown converter tests.
#include <QtTest/QtTest>

#include "markdown/MarkdownConverter.h"

using namespace cf;
using namespace cf::markdown;

class TestMarkdown : public QObject {
    Q_OBJECT
private slots:
    void headings();
    void boldItalicStrike();
    void codeSpansAndBlocks();
    void linksAndImages();
    void listsAndTasks();
    void blockquoteNested();
    void tableRendering();
    void hrAndParagraphs();
    void htmlEscaping();
};

void TestMarkdown::headings()
{
    const QString html = toHtml(QStringLiteral("# Title\n## Sub title\n###### H6"));
    QVERIFY(html.contains("<h1>Title</h1>"));
    QVERIFY(html.contains("<h2>Sub title</h2>"));
    QVERIFY(html.contains("<h6>H6</h6>"));
}

void TestMarkdown::boldItalicStrike()
{
    const QString html = toHtml(QStringLiteral("**bold** and *italic* and ~~gone~~ and __b2__"));
    QVERIFY(html.contains("<strong>bold</strong>"));
    QVERIFY(html.contains("<em>italic</em>"));
    QVERIFY(html.contains("<del>gone</del>"));
    QVERIFY(html.contains("<strong>b2</strong>"));
}

void TestMarkdown::codeSpansAndBlocks()
{
    const QString html = toHtml(QStringLiteral("Use `x = 1` inline.\n\n```cpp\nint main() { return 0; }\n```\n"));
    QVERIFY(html.contains("<code>x = 1</code>"));
    QVERIFY(html.contains("language-cpp"));
    QVERIFY(html.contains("int main() { return 0; }"));
    // No bold interpretation inside code block.
    QVERIFY(!html.contains("<strong>"));
}

void TestMarkdown::linksAndImages()
{
    const QString html = toHtml(QStringLiteral("[Z.ai](https://z.ai) and ![alt text](img.png)"));
    QVERIFY(html.contains("<a href=\"https://z.ai\">Z.ai</a>"));
    QVERIFY(html.contains("<img alt=\"alt text\" src=\"img.png\" />"));
    // Autolink
    const QString html2 = toHtml(QStringLiteral("See <https://example.com>"));
    QVERIFY(html2.contains("<a href=\"https://example.com\">"));
}

void TestMarkdown::listsAndTasks()
{
    const QString html = toHtml(QStringLiteral("- one\n- two\n- three\n"));
    QVERIFY(html.contains("<ul>"));
    QVERIFY(html.contains("<li>one</li>"));
    QVERIFY(html.contains("<li>three</li>"));

    const QString ol = toHtml(QStringLiteral("1. first\n2. second\n"));
    QVERIFY(ol.contains("<ol>"));
    QVERIFY(ol.contains("<li>second</li>"));

    const QString tasks = toHtml(QStringLiteral("- [ ] open\n- [x] done\n"));
    QVERIFY(tasks.contains("type=\"checkbox\" disabled"));
    QVERIFY(tasks.contains(" checked"));
}

void TestMarkdown::blockquoteNested()
{
    const QString html = toHtml(QStringLiteral("> quoted text\n> more\n"));
    QVERIFY(html.contains("<blockquote>"));
    QVERIFY(html.contains("quoted text"));
}

void TestMarkdown::tableRendering()
{
    const QString md = QStringLiteral("| A | B |\n| --- | ---: |\n| 1 | 2 |\n");
    const QString html = toHtml(md);
    QVERIFY(html.contains("<table>"));
    QVERIFY(html.contains("<th>A</th>"));
    QVERIFY(html.contains("text-align:right"));
    QVERIFY(html.contains("<td>1</td>"));                       // left-aligned by default
    QVERIFY(html.contains(">2</td>"));                          // right-aligned via ---:
}

void TestMarkdown::hrAndParagraphs()
{
    const QString html = toHtml(QStringLiteral("para one\n\n---\n\npara two"));
    QVERIFY(html.contains("<p>para one</p>"));
    QVERIFY(html.contains("<hr />"));
    QVERIFY(html.contains("<p>para two</p>"));
}

void TestMarkdown::htmlEscaping()
{
    const QString html = toHtml(QStringLiteral("<script>alert(1)</script> & \"quotes\""));
    QVERIFY(!html.contains("<script>"));
    QVERIFY(html.contains("&lt;script&gt;"));
    QVERIFY(html.contains("&amp;"));
}

QTEST_GUILESS_MAIN(TestMarkdown)
#include "test_markdown.moc"
