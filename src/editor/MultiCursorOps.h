#pragma once
// Multi-cursor editing operations on a set of QTextCursors sharing one
// QTextDocument. Pure cursor math (no widget), fully unit-testable.
//
// All editing operations group into a single undo step and leave every
// cursor positioned after its edit (like VS Code).
#include <QList>
#include <QPair>
#include <QString>

class QTextCursor;
class QTextDocument;

namespace cf {
namespace multicursor {

// Sort cursors ascending by position and drop duplicates/overlaps.
void normalize(QList<QTextCursor>& cursors);

// Insert `text` at every cursor (replacing its selection, if any).
void insertText(QList<QTextCursor>& cursors, const QString& text);

// Backspace at every cursor: remove selection, previous char, or one
// indent unit when sitting after leading indentation.
void backspace(QList<QTextCursor>& cursors, int tabSize, bool insertSpaces);

// Delete forward at every cursor (selection or next character).
void deleteForward(QList<QTextCursor>& cursors);

// Insert newline + auto-indent (copies leading whitespace of the cursor's
// line) at every cursor.
void newline(QList<QTextCursor>& cursors);

// Tab / Shift+Tab over each cursor's selection (or insert indent unit).
void indent(QList<QTextCursor>& cursors, int tabSize, bool insertSpaces, bool more);

// Remove every cursor's selection (used by multi-cursor cut).
void removeSelections(QList<QTextCursor>& cursors);

// Find the next occurrence of `needle` at or after `from`, whole-word aware,
// skipping ranges already taken by other cursors. Returns document position
// or -1.
int nextOccurrence(QTextDocument* doc, const QString& needle, int from, bool wholeWord,
                   const QList<QPair<int, int>>& taken);

// Word span containing `pos` (empty range when not on a word character).
QPair<int, int> wordRangeAt(QTextDocument* doc, int pos);

}  // namespace multicursor
}  // namespace cf
