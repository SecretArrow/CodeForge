#include "editor/MultiCursorOps.h"

#include <QTextBlock>
#include <QTextDocument>
#include <QTextCursor>

namespace cf {
namespace multicursor {

namespace {

bool positionLessThan(const QTextCursor& a, const QTextCursor& b)
{
    return a.selectionStart() < b.selectionStart();
}

QString indentUnit(int tabSize, bool insertSpaces)
{
    return insertSpaces ? QString(qMax(1, tabSize), QLatin1Char(' ')) : QStringLiteral("\t");
}

QString leadingWhitespace(const QString& line)
{
    QString ws;
    for (const QChar ch : line) {
        if (ch == u' ' || ch == u'\t') ws += ch;
        else break;
    }
    return ws;
}

}  // namespace

void normalize(QList<QTextCursor>& cursors)
{
    std::stable_sort(cursors.begin(), cursors.end(), positionLessThan);

    // Drop cursors whose range overlaps the previous one.
    QList<QTextCursor> unique;
    int lastEnd = -1;
    for (const QTextCursor& c : cursors) {
        const int start = c.selectionStart();
        const int end = c.selectionEnd();
        if (start >= lastEnd || !c.hasSelection()) {
            unique.append(c);
            lastEnd = qMax(lastEnd, end);
        }
    }
    cursors = unique;
}

void insertText(QList<QTextCursor>& cursors, const QString& text)
{
    if (cursors.isEmpty()) return;
    QTextCursor anchor(cursors.first());
    anchor.beginEditBlock();

    // Edit from the end backwards so positions stay valid.
    std::stable_sort(cursors.begin(), cursors.end(), positionLessThan);
    for (int i = cursors.size() - 1; i >= 0; --i) {
        QTextCursor c = cursors.at(i);
        c.beginEditBlock();
        if (c.hasSelection()) c.removeSelectedText();
        c.insertText(text);
        c.endEditBlock();
        cursors[i] = c;
    }
    anchor.endEditBlock();
}

void backspace(QList<QTextCursor>& cursors, int tabSize, bool insertSpaces)
{
    if (cursors.isEmpty()) return;
    QTextCursor anchor(cursors.first());
    anchor.beginEditBlock();

    std::stable_sort(cursors.begin(), cursors.end(), positionLessThan);
    for (int i = cursors.size() - 1; i >= 0; --i) {
        QTextCursor c = cursors.at(i);
        c.beginEditBlock();
        if (c.hasSelection()) {
            c.removeSelectedText();
        } else if (c.positionInBlock() > 0) {
            // Delete one indent unit when the cursor sits right after leading whitespace.
            const QString before = c.block().text().left(c.positionInBlock());
            const QString unit = indentUnit(tabSize, insertSpaces);
            const bool onlyWhitespace = leadingWhitespace(before).size() == before.size();
            if (onlyWhitespace && before.size() >= unit.size() && before.endsWith(unit) && unit != QLatin1String(" ")) {
                c.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor, unit.size());
                c.removeSelectedText();
            } else {
                c.deletePreviousChar();
            }
        } else if (c.block().blockNumber() > 0) {
            // Join with previous line.
            c.deletePreviousChar();
        }
        c.endEditBlock();
        cursors[i] = c;
    }
    anchor.endEditBlock();
}

void deleteForward(QList<QTextCursor>& cursors)
{
    if (cursors.isEmpty()) return;
    QTextCursor anchor(cursors.first());
    anchor.beginEditBlock();

    std::stable_sort(cursors.begin(), cursors.end(), positionLessThan);
    for (int i = cursors.size() - 1; i >= 0; --i) {
        QTextCursor c = cursors.at(i);
        c.beginEditBlock();
        if (c.hasSelection())
            c.removeSelectedText();
        else
            c.deleteChar();
        c.endEditBlock();
        cursors[i] = c;
    }
    anchor.endEditBlock();
}

void newline(QList<QTextCursor>& cursors)
{
    if (cursors.isEmpty()) return;
    QTextCursor anchor(cursors.first());
    anchor.beginEditBlock();

    std::stable_sort(cursors.begin(), cursors.end(), positionLessThan);
    for (int i = cursors.size() - 1; i >= 0; --i) {
        QTextCursor c = cursors.at(i);
        c.beginEditBlock();
        if (c.hasSelection()) c.removeSelectedText();
        const QString indent = leadingWhitespace(c.block().text());
        c.insertText(QStringLiteral("\n") + indent);
        c.endEditBlock();
        cursors[i] = c;
    }
    anchor.endEditBlock();
}

void indent(QList<QTextCursor>& cursors, int tabSize, bool insertSpaces, bool more)
{
    if (cursors.isEmpty()) return;
    const QString unit = indentUnit(tabSize, insertSpaces);
    QTextCursor anchor(cursors.first());
    anchor.beginEditBlock();

    std::stable_sort(cursors.begin(), cursors.end(), positionLessThan);
    for (int i = cursors.size() - 1; i >= 0; --i) {
        QTextCursor c = cursors.at(i);
        c.beginEditBlock();
        if (!c.hasSelection()) {
            if (more) {
                c.insertText(unit);
            } else {
                const QString t = c.block().text();
                int remove = 0;
                if (t.startsWith(QLatin1Char('\t'))) remove = 1;
                else for (int k = 0; k < tabSize && k < t.size() && t.at(k) == u' '; ++k) ++remove;
                if (remove > 0) {
                    c.movePosition(QTextCursor::StartOfBlock);
                    c.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, remove);
                    c.removeSelectedText();
                }
            }
        } else {
            const int startBlock = c.document()->findBlock(c.selectionStart()).blockNumber();
            const int endBlock = c.document()->findBlock(c.selectionEnd()).blockNumber();
            for (int n = endBlock; n >= startBlock; --n) {
                QTextBlock b = c.document()->findBlockByNumber(n);
                if (!b.isValid() || b.text().trimmed().isEmpty()) continue;
                QTextCursor bc(b);
                bc.movePosition(QTextCursor::StartOfBlock);
                if (more) {
                    bc.insertText(unit);
                } else {
                    const QString t = b.text();
                    int remove = 0;
                    if (t.startsWith(QLatin1Char('\t'))) remove = 1;
                    else for (int k = 0; k < tabSize && k < t.size() && t.at(k) == u' '; ++k) ++remove;
                    if (remove > 0) {
                        bc.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, remove);
                        bc.removeSelectedText();
                    }
                }
            }
            // Clamp selection back inside its original bounds.
            if (c.selectionEnd() > c.document()->characterCount())
                c.movePosition(QTextCursor::End);
        }
        c.endEditBlock();
        cursors[i] = c;
    }
    anchor.endEditBlock();
}

void removeSelections(QList<QTextCursor>& cursors)
{
    if (cursors.isEmpty()) return;
    QTextCursor anchor(cursors.first());
    anchor.beginEditBlock();

    std::stable_sort(cursors.begin(), cursors.end(), positionLessThan);
    for (int i = cursors.size() - 1; i >= 0; --i) {
        QTextCursor c = cursors.at(i);
        c.beginEditBlock();
        if (c.hasSelection()) c.removeSelectedText();
        c.endEditBlock();
        cursors[i] = c;
    }
    anchor.endEditBlock();
}

QPair<int, int> wordRangeAt(QTextDocument* doc, int pos)
{
    if (!doc || pos < 0 || pos >= doc->characterCount()) return {-1, -1};
    const QChar at = doc->characterAt(pos);
    if (!at.isLetterOrNumber() && at != u'_') return {-1, -1};

    int start = pos;
    while (start > 0) {
        const QChar ch = doc->characterAt(start - 1);
        if (!ch.isLetterOrNumber() && ch != u'_') break;
        --start;
    }
    int end = pos;
    while (end < doc->characterCount()) {
        const QChar ch = doc->characterAt(end);
        if (!ch.isLetterOrNumber() && ch != u'_') break;
        ++end;
    }
    return {start, end};
}

int nextOccurrence(QTextDocument* doc, const QString& needle, int from, bool wholeWord,
                   const QList<QPair<int, int>>& taken)
{
    if (!doc || needle.isEmpty()) return -1;
    const int docLen = int(doc->characterCount());
    const QString text = doc->toPlainText();
    int pos = qMax(0, from);
    while (pos <= docLen - needle.size()) {
        pos = int(text.indexOf(needle, pos));
        if (pos < 0) return -1;
        const int end = pos + needle.size();

        bool overlaps = false;
        for (const auto& r : taken) {
            if (pos < r.second && r.first < end) { overlaps = true; break; }
        }
        if (!overlaps) {
            bool boundary = true;
            if (wholeWord) {
                const QChar before = pos > 0 ? doc->characterAt(pos - 1) : QChar();
                const QChar after = end < docLen ? doc->characterAt(end) : QChar();
                boundary = !(before.isLetterOrNumber() || before == u'_') &&
                           !(after.isLetterOrNumber() || after == u'_');
            }
            if (boundary) return pos;
        }
        ++pos;
    }
    return -1;
}

}  // namespace multicursor
}  // namespace cf
