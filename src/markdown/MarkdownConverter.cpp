#include "markdown/MarkdownConverter.h"

#include <QRegularExpression>
#include <QStringList>

namespace cf {
namespace markdown {

namespace {

// ---- inline rendering helpers (internal) ----

QString renderInline(const QString& raw)
{
    QString text = escapeHtml(raw);

    // Protect inline code spans first (placeholders), so other rules skip them.
    QStringList codeSpans;
    static const QRegularExpression codeRe(QStringLiteral("`([^`]+)`"));
    QRegularExpressionMatchIterator it = codeRe.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        codeSpans.append(QStringLiteral("<code>%1</code>").arg(m.captured(1)));
        text.replace(m.captured(0), QStringLiteral("\x01CF%1\x02").arg(codeSpans.size() - 1));
    }

    // Images before links (image syntax is a superset prefix).
    static const QRegularExpression imageRe(QStringLiteral("!\\[([^\\]]*)\\]\\(([^)\\s]+)\\)"));
    text.replace(imageRe, QStringLiteral(R"(<img alt="\1" src="\2" />)"));

    static const QRegularExpression linkRe(QStringLiteral("\\[([^\\]]+)\\]\\(([^)\\s]+)\\)"));
    text.replace(linkRe, QStringLiteral(R"(<a href="\2">\1</a>)"));

    static const QRegularExpression autolinkRe(QStringLiteral("&lt;(https?://[^&\\s]+)&gt;"));
    text.replace(autolinkRe, QStringLiteral(R"(<a href="\1">\1</a>)"));

    static const QRegularExpression boldRe(QStringLiteral("\\*\\*(.+?)\\*\\*|__(.+?)__"));
    {
        QString out;
        int last = 0;
        auto it2 = boldRe.globalMatch(text);
        while (it2.hasNext()) {
            const QRegularExpressionMatch m = it2.next();
            out += text.mid(last, m.capturedStart() - last);
            out += QStringLiteral("<strong>%1</strong>").arg(m.captured(1).isEmpty() ? m.captured(2) : m.captured(1));
            last = m.capturedEnd();
        }
        out += text.mid(last);
        text = out;
    }
    static const QRegularExpression italicRe(QStringLiteral("(?<!\\*)\\*([^*\\s][^*]*?)\\*(?!\\*)|(?<!_)_([^_\\s][^_]*?)_(?!_)"));
    {
        QString out;
        int last = 0;
        auto it2 = italicRe.globalMatch(text);
        while (it2.hasNext()) {
            const QRegularExpressionMatch m = it2.next();
            out += text.mid(last, m.capturedStart() - last);
            out += QStringLiteral("<em>%1</em>").arg(m.captured(1).isEmpty() ? m.captured(2) : m.captured(1));
            last = m.capturedEnd();
        }
        out += text.mid(last);
        text = out;
    }
    static const QRegularExpression strikeRe(QStringLiteral("~~(.+?)~~"));
    text.replace(strikeRe, QStringLiteral("<del>\\1</del>"));

    // Restore code spans.
    for (int i = 0; i < codeSpans.size(); ++i)
        text.replace(QStringLiteral("\x01CF%1\x02").arg(i), codeSpans.at(i));

    return text;
}

bool isHr(const QString& line)
{
    static const QRegularExpression hrRe(QStringLiteral("^ {0,3}((\\*[ ]*){3,}|(-[ ]*){3,}|(_[ ]*){3,})$"));
    return hrRe.match(line).hasMatch();
}

QString renderListBlock(const QStringList& lines, bool ordered);

bool isListItem(const QString& line, bool* orderedOut = nullptr)
{
    static const QRegularExpression ulRe(QStringLiteral(R"(^(\s*)[-*+]\s+(.*)$)"));
    static const QRegularExpression olRe(QStringLiteral(R"(^(\s*)\d+[.)]\s+(.*)$)"));
    const auto ul = ulRe.match(line);
    if (ul.hasMatch()) { if (orderedOut) *orderedOut = false; return true; }
    const auto ol = olRe.match(line);
    if (ol.hasMatch()) { if (orderedOut) *orderedOut = true; return true; }
    return false;
}

QString renderListBlock(const QStringList& lines, bool ordered)
{
    QString body;
    for (const QString& line : lines) {
        static const QRegularExpression itemRe(QStringLiteral(R"(^\s*(?:[-*+]|\d+[.)])\s+(.*)$)"));
        const auto m = itemRe.match(line);
        if (!m.hasMatch()) continue;   // continuation line: appended below
        QString content = m.captured(1);

        // Task list checkboxes.
        static const QRegularExpression taskRe(QStringLiteral("^\\[( |x|X)\\]\\s+(.*)$"));
        const auto task = taskRe.match(content);
        if (task.hasMatch()) {
            const bool checked = task.captured(1).compare(QLatin1String("x"), Qt::CaseInsensitive) == 0;
            content = QStringLiteral(R"(<input type="checkbox" disabled%1> %2)")
                          .arg(checked ? QStringLiteral(" checked") : QString(), renderInline(task.captured(2)));
        } else {
            content = renderInline(content);
        }
        body += QStringLiteral("<li>%1</li>").arg(content);
    }
    return ordered ? QStringLiteral("<ol>%1</ol>").arg(body) : QStringLiteral("<ul>%1</ul>").arg(body);
}

}  // namespace

QString escapeHtml(const QString& text)
{
    QString out;
    out.reserve(text.size() + 8);
    for (const QChar c : text) {
        switch (c.unicode()) {
        case u'&': out += QStringLiteral("&amp;"); break;
        case u'<': out += QStringLiteral("&lt;"); break;
        case u'>': out += QStringLiteral("&gt;"); break;
        case u'"': out += QStringLiteral("&quot;"); break;
        default: out += c; break;
        }
    }
    return out;
}

QString toHtml(const QString& source)
{
    const QStringList lines = source.split(QLatin1Char('\n'));
    QString html;
    int i = 0;

    while (i < lines.size()) {
        const QString line = lines.at(i);
        const QString trimmed = line.trimmed();

        // ---- fenced code ----
        if (trimmed.startsWith(QLatin1String("```")) || trimmed.startsWith(QLatin1String("~~~"))) {
            const QString fence = trimmed.left(3);
            const QString lang = trimmed.mid(3).trimmed();
            ++i;
            QStringList code;
            while (i < lines.size() && !lines.at(i).trimmed().startsWith(fence)) {
                code << lines.at(i);
                ++i;
            }
            ++i;   // closing fence
            const QString cls = lang.isEmpty() ? QString()
                                               : QStringLiteral(" class=\"language-%1\"").arg(escapeHtml(lang));
            html += QStringLiteral("<pre><code%1>%2</code></pre>\n").arg(cls, escapeHtml(code.join(QLatin1Char('\n'))));
            continue;
        }

        // ---- blank ----
        if (trimmed.isEmpty()) { ++i; continue; }

        // ---- heading ----
        static const QRegularExpression hRe(QStringLiteral("^(#{1,6})\\s+(.*)$"));
        const auto h = hRe.match(line);
        if (h.hasMatch()) {
            const int level = h.captured(1).size();
            html += QStringLiteral("<h%1>%2</h%1>\n").arg(level).arg(renderInline(h.captured(2).trimmed()));
            ++i;
            continue;
        }

        // ---- horizontal rule ----
        if (isHr(line)) {
            html += QStringLiteral("<hr />\n");
            ++i;
            continue;
        }

        // ---- table ----
        if (trimmed.startsWith(QLatin1Char('|')) && i + 1 < lines.size()) {
            const QString sep = lines.at(i + 1).trimmed();
            static const QRegularExpression sepRe(
                QStringLiteral("^\\|(\\s*:?-+:?\\s*\\|)+\\s*:?-+:?\\s*\\|?$"));
            if (sepRe.match(sep).hasMatch()) {
                const auto splitRow = [](const QString& row) {
                    QStringList cells;
                    QString cur;
                    for (int k = 1; k < row.size(); ++k) {   // skip leading |
                        const QChar c = row.at(k);
                        if (c == u'|') { cells << cur.trimmed(); cur.clear(); }
                        else cur += c;
                    }
                    if (!cur.isEmpty() || row.endsWith(QLatin1Char('|'))) cells << cur.trimmed();
                    return cells;
                };
                const QStringList header = splitRow(trimmed);
                static const QRegularExpression alignRe(QStringLiteral(":?-+:?"));
                QStringList aligns;
                for (const QString& cell : splitRow(sep)) {
                    const bool left = cell.startsWith(QLatin1Char(':'));
                    const bool right = cell.endsWith(QLatin1Char(':'));
                    aligns << (left && right ? QStringLiteral("center") : right ? QStringLiteral("right")
                                                                               : left ? QStringLiteral("left") : QString());
                }
                html += QStringLiteral("<table>\n<thead>\n<tr>");
                for (int cIdx = 0; cIdx < header.size(); ++cIdx) {
                    const QString a = cIdx < aligns.size() ? aligns.at(cIdx) : QString();
                    html += a.isEmpty() ? QStringLiteral("<th>%1</th>").arg(renderInline(header.at(cIdx)))
                                        : QStringLiteral("<th style=\"text-align:%1\">%2</th>").arg(a, renderInline(header.at(cIdx)));
                }
                html += QStringLiteral("</tr>\n</thead>\n<tbody>\n");
                i += 2;
                while (i < lines.size() && lines.at(i).trimmed().startsWith(QLatin1Char('|'))) {
                    const QStringList row = splitRow(lines.at(i).trimmed());
                    html += QStringLiteral("<tr>");
                    for (int cIdx = 0; cIdx < header.size(); ++cIdx) {
                        const QString a = cIdx < aligns.size() ? aligns.at(cIdx) : QString();
                        const QString cell = cIdx < row.size() ? row.at(cIdx) : QString();
                        html += a.isEmpty() ? QStringLiteral("<td>%1</td>").arg(renderInline(cell))
                                            : QStringLiteral("<td style=\"text-align:%1\">%2</td>").arg(a, renderInline(cell));
                    }
                    html += QStringLiteral("</tr>\n");
                    ++i;
                }
                html += QStringLiteral("</tbody>\n</table>\n");
                continue;
            }
        }

        // ---- blockquote ----
        if (trimmed.startsWith(QLatin1Char('>'))) {
            QStringList inner;
            while (i < lines.size() && lines.at(i).trimmed().startsWith(QLatin1Char('>'))) {
                QString l = lines.at(i);
                l.remove(0, l.indexOf(QLatin1Char('>')) + 1);
                if (l.startsWith(QLatin1Char(' '))) l.remove(0, 1);
                inner << l;
                ++i;
            }
            html += QStringLiteral("<blockquote>\n%1</blockquote>\n").arg(toHtml(inner.join(QLatin1Char('\n'))));
            continue;
        }

        // ---- lists ----
        bool ordered = false;
        if (isListItem(line, &ordered)) {
            QStringList block;
            while (i < lines.size()) {
                const QString& l = lines.at(i);
                if (l.trimmed().isEmpty() || isHr(l)) break;
                bool liOrdered = false;
                if (isListItem(l, &liOrdered)) {
                    if (liOrdered != ordered && !l.startsWith(QStringLiteral("  ")) && !l.startsWith(QLatin1Char('\t'))) break;
                    block << l;
                } else if (l.startsWith(QStringLiteral("    ")) || l.startsWith(QLatin1Char('\t'))) {
                    block << l;   // indented continuation
                } else {
                    break;
                }
                ++i;
            }
            html += renderListBlock(block, ordered) + QLatin1Char('\n');
            continue;
        }

        // ---- paragraph (consecutive non-empty, non-special lines) ----
        QStringList para;
        while (i < lines.size()) {
            const QString& l = lines.at(i);
            const QString t = l.trimmed();
            if (t.isEmpty() || isHr(l) || hRe.match(l).hasMatch() || t.startsWith(QLatin1String("```")) ||
                t.startsWith(QLatin1String("~~~")) || t.startsWith(QLatin1Char('>')) || isListItem(l) ||
                t.startsWith(QLatin1Char('|')))
                break;
            para << l;
            ++i;
        }
        if (!para.isEmpty())
            html += QStringLiteral("<p>%1</p>\n").arg(renderInline(para.join(QLatin1Char('\n')).trimmed()));
        else
            ++i;   // safety: never loop forever
    }
    return html;
}

}  // namespace markdown
}  // namespace cf
