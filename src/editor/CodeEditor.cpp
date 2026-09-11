#include "editor/CodeEditor.h"

#include <QAbstractScrollArea>
#include <QApplication>
#include <QFontDatabase>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSet>
#include <QTextBlock>
#include <QTextLayout>

#include "editor/Minimap.h"
#include "settings/SettingsManager.h"
#include "syntax/LanguageRegistry.h"
#include "themes/Theme.h"

namespace cf {

// ---------------- LineNumberArea ----------------

LineNumberArea::LineNumberArea(CodeEditor* editor)
    : QWidget(editor), m_editor(editor)
{
    setObjectName(QStringLiteral("codeforge_linenumberarea"));
}

QSize LineNumberArea::sizeHint() const
{
    return QSize(m_editor->marginWidth(), 0);
}

void LineNumberArea::paintEvent(QPaintEvent* event)
{
    m_editor->paintMargin(event);
}

void LineNumberArea::mousePressEvent(QMouseEvent* event)
{
    // Click on the fold zone toggles folding for that line.
    const QTextBlock block = m_editor->blockForPos(event->pos().y());
    if (block.isValid() && m_editor->m_showFolding)
        m_editor->toggleFold(block.blockNumber());
}

// ---------------- CodeEditor ----------------

CodeEditor::CodeEditor(TextDocument* doc, QWidget* parent)
    : QPlainTextEdit(parent), m_doc(doc), m_margin(new LineNumberArea(this))
{
    setDocument(doc->document());
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setFrameStyle(QFrame::NoFrame);
    setObjectName(QStringLiteral("codeforge_editor"));

    m_minimap = new Minimap(this);

    connect(document(), &QTextDocument::blockCountChanged, this, [this](int) { updateMarginWidth(); });
    connect(document(), &QTextDocument::documentLayoutChanged, this, [this]() {
        m_margin->update();
        applyFoldVisibility();
    });
    connect(this, &QPlainTextEdit::updateRequest, this, &CodeEditor::updateMargin);
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, &CodeEditor::onCursorMoved);
    connect(this, &QPlainTextEdit::textChanged, this, &CodeEditor::onDocChanged);
    connect(this, &QPlainTextEdit::blockCountChanged, this, [this](int) { if (m_minimap) m_minimap->scheduleRebuild(); });

    updateMarginWidth();
    onCursorMoved();
}

void CodeEditor::applySettings()
{
    SettingsManager& s = SettingsManager::instance();

    QFont f(s.getString(QStringLiteral("editor.fontFamily")));
    if (QFontDatabase::hasFamily(f.family())) {
        f.setStyleHint(QFont::Monospace);
    } else {
        f = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    }
    f.setPointSize(s.getInt(QStringLiteral("editor.fontSize")));
    f.setWeight(s.getInt(QStringLiteral("editor.fontWeight")) == 700 ? QFont::Bold : QFont::Normal);
    setFont(f);
    m_margin->setFont(f);
    if (m_minimap) m_minimap->refreshFont(f);

    m_tabSize = qBound(1, s.getInt(QStringLiteral("editor.tabSize")), 16);
    m_insertSpaces = s.getBool(QStringLiteral("editor.insertSpaces"));
    setTabStopDistance(fontMetrics().horizontalAdvance(QLatin1Char(' ')) * m_tabSize);
    setLineWrapMode(s.getBool(QStringLiteral("editor.wordWrap")) ? QPlainTextEdit::WidgetWidth
                                                                 : QPlainTextEdit::NoWrap);

    const QString ws = s.getString(QStringLiteral("editor.renderWhitespace"));
    QTextOption opt = document()->defaultTextOption();
    if (ws == QLatin1String("all"))
        opt.setFlags(opt.flags() | QTextOption::ShowTabsAndSpaces | QTextOption::ShowLineAndParagraphSeparators);
    else if (ws == QLatin1String("boundary"))
        opt.setFlags(opt.flags() | QTextOption::ShowTabsAndSpaces);
    else
        opt.setFlags(opt.flags() & ~(QTextOption::ShowTabsAndSpaces | QTextOption::ShowLineAndParagraphSeparators));
    document()->setDefaultTextOption(opt);

    const QString cursorStyle = s.getString(QStringLiteral("editor.cursorStyle"));
    if (cursorStyle == QLatin1String("block"))
        setCursorWidth(fontMetrics().horizontalAdvance(QLatin1Char('X')));
    else if (cursorStyle == QLatin1String("underline"))
        setCursorWidth(qMax(2, fontMetrics().horizontalAdvance(QLatin1Char('X')) / 6));
    else
        setCursorWidth(1);

    QApplication::setCursorFlashTime(s.getBool(QStringLiteral("editor.cursorBlinking")) ? 1060 : 0);

    m_showLineNumbers = s.getBool(QStringLiteral("editor.showLineNumbers"));
    m_showFolding = s.getBool(QStringLiteral("editor.showFolding"));
    m_indentGuides = s.getBool(QStringLiteral("editor.indentGuides"));
    m_highlightActiveLine = s.getBool(QStringLiteral("editor.highlightActiveLine"));
    m_bracketMatching = s.getBool(QStringLiteral("editor.bracketMatching"));
    m_autoIndent = s.getBool(QStringLiteral("editor.autoIndent"));
    m_autoClose = s.getBool(QStringLiteral("editor.autoClosingBrackets"));
    setReadOnly(m_doc->isReadOnly());

    setViewportMargins(marginWidth(), 0, 0, 0);
    updateExtraSelections();
    m_margin->update();

    if (!m_lineHeightApplied) {
        const double lh = s.get(QStringLiteral("editor.lineHeight")).toDouble();
        if (lh > 1.0 && document()->blockCount() < 200000) {
            const int px = int(fontMetrics().height() * (lh - 1.0));
            if (px > 0) {
                QTextCursor c(document());
                c.select(QTextCursor::Document);
                QTextBlockFormat fmt;
                fmt.setBottomMargin(px / 2.0);
                fmt.setTopMargin(px / 2.0);
                c.mergeBlockFormat(fmt);
            }
            m_lineHeightApplied = true;
        }
    }
}

void CodeEditor::applyTheme(const Theme& t)
{
    m_activeLineColor = t.color(QStringLiteral("editor.activeLine"));
    m_lineNumberColor = t.color(QStringLiteral("editor.lineNumber"));
    m_lineNumberActiveColor = t.color(QStringLiteral("editor.lineNumberActive"));
    m_indentGuideColor = t.color(QStringLiteral("editor.indentGuide"));
    m_bracketMatchColor = t.color(QStringLiteral("editor.bracketMatch"));
    m_findMatchColor = t.color(QStringLiteral("editor.findMatch"));
    m_currentFindColor = t.color(QStringLiteral("editor.currentFindMatch"));
    m_foldArrowColor = t.color(QStringLiteral("editor.lineNumber"));

    QPalette pal = viewport()->palette();
    pal.setColor(QPalette::Base, t.editorBackground());
    pal.setColor(QPalette::Text, t.editorForeground());
    pal.setColor(QPalette::Highlight, t.color(QStringLiteral("editor.selection")));
    pal.setColor(QPalette::HighlightedText, t.editorForeground());
    viewport()->setPalette(pal);
    setPalette(pal);

    updateExtraSelections();
    m_margin->update();
}

void CodeEditor::updateMarginWidth()
{
    setViewportMargins(marginWidth(), 0, 0, 0);
}

int CodeEditor::marginWidth() const
{
    int digits = 2;
    int max = qMax(1, blockCount());
    while (max >= 10) { max /= 10; ++digits; }
    digits = qMax(digits, 2);

    int w = fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits + 10;
    if (m_showFolding) w += 14;
    if (!m_showLineNumbers) w = m_showFolding ? 16 : 0;
    return w;
}

void CodeEditor::updateMargin(const QRect& rect, int dy)
{
    if (dy) m_margin->scroll(0, dy);
    else m_margin->update(0, rect.y(), m_margin->width(), rect.height());
    if (rect.contains(viewport()->rect())) updateMarginWidth();
}

void CodeEditor::resizeEvent(QResizeEvent* e)
{
    QPlainTextEdit::resizeEvent(e);
    QRect cr = contentsRect();
    m_margin->setGeometry(cr.left(), cr.top(), marginWidth(), cr.height());
    if (m_minimap) m_minimap->syncViewport();
}

void CodeEditor::paintMargin(QPaintEvent* e)
{
    QPainter p(m_margin);
    p.fillRect(e->rect(), palette().window().color());

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());
    const QFontMetrics fm(font());
    const int foldZoneStart = marginWidth() - 14;
    const int currentLine = textCursor().blockNumber();

    while (block.isValid() && top <= e->rect().bottom()) {
        if (block.isVisible() && m_showLineNumbers) {
            p.setPen(blockNumber == currentLine ? m_lineNumberActiveColor : m_lineNumberColor);
            p.drawText(0, top, m_margin->width() - 16, fm.height(),
                       Qt::AlignRight, QString::number(blockNumber + 1));
        }
        // Fold indicator: arrow when this block starts a foldable range.
        if (m_showFolding && block.isVisible() && foldRangeForBlock(blockNumber, nullptr)) {
            p.save();
            p.setRenderHint(QPainter::Antialiasing);
            QPen pen(m_foldArrowColor, 1.4);
            p.setPen(pen);
            const int cx = foldZoneStart + 7;
            const int cy = top + fm.ascent() / 2;
            if (m_foldedBlocks.contains(blockNumber)) {
                p.drawLine(cx - 2, cy - 3, cx + 2, cy);
                p.drawLine(cx + 2, cy, cx - 2, cy + 3);
            } else {
                p.drawLine(cx - 3, cy - 1, cx, cy + 2);
                p.drawLine(cx, cy + 2, cx + 3, cy - 1);
            }
            p.restore();
        }
        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }

    // Right border of the margin area.
    p.setPen(palette().window().color().darker(115));
    p.drawLine(m_margin->width() - 1, e->rect().top(), m_margin->width() - 1, e->rect().bottom());
}

QTextBlock CodeEditor::blockForPos(int y) const
{
    // Exposed to LineNumberArea through friend.
    QTextBlock block = firstVisibleBlock();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    while (block.isValid()) {
        const int height = qRound(blockBoundingRect(block).height());
        if (y >= top && y < top + height) return block;
        top += height;
        block = block.next();
    }
    return QTextBlock();
}

// ---------------- folding ----------------

bool CodeEditor::foldRangeForBlock(int blockNumber, FoldRange* out) const
{
    const QString lang = m_doc ? m_doc->languageId() : QString();
    QTextBlock block = document()->findBlockByNumber(blockNumber);
    if (!block.isValid()) return false;
    const QString text = block.text();

    if (LanguageRegistry::instance().hasBraceFolding(lang)) {
        const int open = text.indexOf(QLatin1Char('{'));
        if (open < 0) return false;
        int depth = 0;
        QTextBlock b = block;
        int line = blockNumber;
        bool sawOpen = false;
        while (b.isValid()) {
            const QString bt = b.text();
            const int from = (b == block) ? open : 0;
            for (int i = from; i < bt.size(); ++i) {
                const QChar ch = bt.at(i);
                if (ch == u'{') { ++depth; sawOpen = true; }
                else if (ch == u'}') {
                    --depth;
                    if (sawOpen && depth == 0) {
                        if (line == blockNumber) return false;   // opens and closes on same line
                        if (out) *out = {blockNumber, line};
                        return true;
                    }
                }
            }
            if (!b.next().isValid()) break;
            b = b.next();
            ++line;
        }
        return false;   // unterminated: don't fold
    }
    if (LanguageRegistry::instance().usesIndentFolding(lang)) {
        const int indent = int(text.size() - text.trimmed().size());
        if (text.trimmed().isEmpty()) return false;
        QTextBlock b = block.next();
        int last = blockNumber;
        while (b.isValid()) {
            const QString bt = b.text();
            if (!bt.trimmed().isEmpty()) {
                const int bi = int(bt.size() - bt.trimmed().size());
                if (bi <= indent) break;
                last = b.blockNumber();
            }
            b = b.next();
        }
        if (last > blockNumber) { if (out) *out = {blockNumber, last}; return true; }
    }
    return false;
}

void CodeEditor::toggleFold(int blockNumber)
{
    if (m_foldedBlocks.contains(blockNumber))
        m_foldedBlocks.remove(blockNumber);
    else
        m_foldedBlocks.insert(blockNumber);
    applyFoldVisibility();
}

void CodeEditor::applyFoldVisibility()
{
    // Hide every block strictly inside folded ranges.
    for (QTextBlock b = document()->firstBlock(); b.isValid(); b = b.next())
        b.setVisible(true);

    QList<FoldRange> ranges;
    for (int start : m_foldedBlocks) {
        FoldRange r;
        if (foldRangeForBlock(start, &r)) ranges.append(r);
    }
    for (const FoldRange& r : ranges) {
        QTextBlock b = document()->findBlockByNumber(r.start + 1);
        int n = r.start + 1;
        while (b.isValid() && n <= r.end) {
            b.setVisible(false);
            b = b.next();
            ++n;
        }
    }
    document()->markContentsDirty(0, 0);
    viewport()->update();
    m_margin->update();
    if (m_minimap) m_minimap->syncViewport();
}

void CodeEditor::unfoldAround(int blockNumber)
{
    bool changed = false;
    QList<int> toRemove;
    for (int start : m_foldedBlocks) {
        FoldRange r;
        if (foldRangeForBlock(start, &r) && blockNumber > r.start && blockNumber <= r.end)
            toRemove.append(start);
    }
    for (int s : toRemove) { m_foldedBlocks.remove(s); changed = true; }
    if (changed) applyFoldVisibility();
}

// ---------------- extra selections / highlighting ----------------

QTextCursor CodeEditor::currentLineSelection() const
{
    QTextCursor c = textCursor();
    c.clearSelection();
    return c;
}

QList<QTextEdit::ExtraSelection> CodeEditor::buildExtraSelections() const
{
    QList<QTextEdit::ExtraSelection> sel;

    if (m_highlightActiveLine && !isReadOnly()) {
        QTextEdit::ExtraSelection es;
        es.format.setBackground(m_activeLineColor);
        es.format.setProperty(QTextFormat::FullWidthSelection, true);
        es.cursor = currentLineSelection();
        sel.append(es);
    }

    // Bracket matching.
    if (m_bracketMatching) {
        const QString openers = QStringLiteral("{[(");
        const QString closers = QStringLiteral("}])");
        int pos = textCursor().position();
        QChar atCursor = document()->characterAt(pos);
        if (atCursor.isNull() || (!openers.contains(atCursor) && !closers.contains(atCursor))) {
            atCursor = document()->characterAt(pos - 1);
            if (!atCursor.isNull() && (openers.contains(atCursor) || closers.contains(atCursor)))
                --pos;
            else
                atCursor = QChar();
        }
        if (!atCursor.isNull()) {
            const bool forward = openers.contains(atCursor);
            QChar match;
            int depth = 0;
            int found = -1;
            const int docLen = int(document()->characterCount());
            if (forward) {
                match = closers.at(openers.indexOf(atCursor));
                for (int i = pos; i < docLen && i - pos < 500000; ++i) {
                    const QChar c = document()->characterAt(i);
                    if (c == atCursor) ++depth;
                    else if (c == match) { --depth; if (depth == 0) { found = i; break; } }
                }
            } else {
                match = openers.at(closers.indexOf(atCursor));
                for (int i = pos; i >= 0 && pos - i < 500000; --i) {
                    const QChar c = document()->characterAt(i);
                    if (c == atCursor) ++depth;
                    else if (c == match) { --depth; if (depth == 0) { found = i; break; } }
                }
            }
            if (found >= 0) {
                for (int p : { pos, found }) {
                    QTextEdit::ExtraSelection es;
                    es.format.setBackground(m_bracketMatchColor);
                    es.cursor = QTextCursor(document());
                    es.cursor.setPosition(p);
                    es.cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
                    sel.append(es);
                }
            }
        }
    }

    // Find matches.
    for (int i = 0; i < m_findMatches.size(); ++i) {
        QTextEdit::ExtraSelection es;
        es.format.setBackground(i == m_currentFind ? m_currentFindColor : m_findMatchColor);
        es.cursor = m_findMatches.at(i);
        sel.append(es);
    }
    return sel;
}

void CodeEditor::updateExtraSelections()
{
    setExtraSelections(buildExtraSelections());
}

void CodeEditor::onCursorMoved()
{
    updateExtraSelections();

    const QTextCursor c = textCursor();
    const int line = c.blockNumber();
    const int col = c.positionInBlock();
    int selChars = 0, selLines = 0;
    if (c.hasSelection()) {
        selChars = c.selectedText().length();
        selLines = c.selectedText().count(QChar(u'\u2029')) + 1;
    }
    unfoldAround(line);
    if (m_minimap) m_minimap->syncViewport();
    emit cursorStats(line, col, selChars, selLines);
}

void CodeEditor::onDocChanged()
{
    // Simple, predictable policy: folds are cleared on edits (recompute is user-driven).
    if (!m_foldedBlocks.isEmpty()) { m_foldedBlocks.clear(); applyFoldVisibility(); }
    if (m_minimap) m_minimap->scheduleRebuild();
}

void CodeEditor::setFindMatches(const QList<QTextCursor>& matches, int currentIndex)
{
    m_findMatches = matches;
    m_currentFind = currentIndex;
    updateExtraSelections();
}

void CodeEditor::clearFindMatches()
{
    m_findMatches.clear();
    m_currentFind = -1;
    updateExtraSelections();
}

// ---------------- navigation ----------------

void CodeEditor::gotoLine(int line, int column)
{
    QTextBlock b = document()->findBlockByNumber(line);
    if (!b.isValid()) b = document()->lastBlock();
    QTextCursor c(b);
    c.movePosition(QTextCursor::StartOfBlock);
    if (column > 0) c.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, qMin(column, b.length() - 1));
    setTextCursor(c);
    centerCursor();
    setFocus();
}

void CodeEditor::increaseFontSize()
{
    QFont f = font();
    f.setPointSize(f.pointSize() + 1);
    setFont(f);
}

void CodeEditor::decreaseFontSize()
{
    QFont f = font();
    if (f.pointSize() > 6) f.setPointSize(f.pointSize() - 1);
    setFont(f);
}

void CodeEditor::focusInEvent(QFocusEvent* e)
{
    QPlainTextEdit::focusInEvent(e);
    emit focusGained();
}

void CodeEditor::contextMenuEvent(QContextMenuEvent* e)
{
    emit contextRequested(e->globalPos());
}

// ---------------- editing ----------------

void CodeEditor::indentSelection(bool more)
{
    QTextCursor c = textCursor();
    const QString unit = m_insertSpaces ? QString(m_tabSize, QLatin1Char(' ')) : QStringLiteral("\t");

    if (!c.hasSelection()) {
        if (more) c.insertText(unit);
        else c.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor, 1);
        return;
    }

    const int startBlock = document()->findBlock(c.selectionStart()).blockNumber();
    const int endBlock = document()->findBlock(c.selectionEnd()).blockNumber();
    c.beginEditBlock();
    for (int n = startBlock; n <= endBlock; ++n) {
        QTextBlock b = document()->findBlockByNumber(n);
        if (!b.isValid() || b.text().trimmed().isEmpty()) continue;
        QTextCursor bc(b);
        bc.movePosition(QTextCursor::StartOfBlock);
        if (more) bc.insertText(unit);
        else {
            const QString t = b.text();
            int remove = 0;
            if (t.startsWith(QStringLiteral("\t"))) remove = 1;
            else for (int i = 0; i < m_tabSize && i < t.size() && t.at(i) == u' '; ++i) ++remove;
            if (remove > 0) { bc.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, remove); bc.removeSelectedText(); }
        }
    }
    c.endEditBlock();
}

void CodeEditor::handleAutoIndent()
{
    if (!m_autoIndent) { textCursor().insertText(QStringLiteral("\n")); return; }

    QTextCursor c = textCursor();
    QTextBlock b = c.block();
    const QString lineText = b.text().left(c.positionInBlock());
    QString indent;
    for (const QChar ch : lineText) {
        if (ch == u' ' || ch == u'\t') indent += ch;
        else break;
    }

    const QString lang = m_doc->languageId();
    const QString trimmed = lineText.trimmed();

    // Extra indent after opening constructs.
    if (trimmed.endsWith(QLatin1Char('{')) || (lang == QLatin1String("python") && trimmed.endsWith(QLatin1Char(':'))))
        indent += m_insertSpaces ? QString(m_tabSize, QLatin1Char(' ')) : QStringLiteral("\t");

    const QChar nextChar = document()->characterAt(c.position());
    if (nextChar == u'}' && trimmed.endsWith(QLatin1Char('{'))) {
        // Smart close: put } on its own line.
        const QString unit = m_insertSpaces ? QString(m_tabSize, QLatin1Char(' ')) : QStringLiteral("\t");
        c.insertText(QStringLiteral("\n") + indent + QStringLiteral("\n") + (indent.size() >= unit.size() ? indent.left(indent.size() - unit.size()) : indent));
        QTextCursor back = c;
        back.setPosition(c.position() - 1 - indent.size());
        setTextCursor(back);
        return;
    }

    c.insertText(QStringLiteral("\n") + indent);
    setTextCursor(c);
}

bool CodeEditor::handleAutoClose(QKeyEvent* e)
{
    if (isReadOnly() || !m_autoClose) return false;
    const QString text = e->text();
    if (text.size() != 1) return false;
    const QChar typed = text.at(0);
    if (!typed.isPrint()) return false;

    QTextCursor c = textCursor();
    const QChar next = document()->characterAt(c.position());

    static const QString openers = QStringLiteral("([{");
    static const QString closers = QStringLiteral(")]}");
    static const QString quotes = QStringLiteral("\"'`");

    // Skip over a typed closer that already exists ahead.
    if (closers.contains(typed) && next == typed) {
        c.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
        c.clearSelection();
        setTextCursor(c);
        return true;
    }

    if (quotes.contains(typed)) {
        if (next == typed) {
            c.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
            c.clearSelection();
            setTextCursor(c);
            return true;
        }
        if (c.hasSelection()) {
            const QString sel = c.selectedText();
            c.insertText(typed + sel + typed);
            return true;
        }
        // Only open pair when not right after a word character (avoid strings like don't).
        const QChar prev = document()->characterAt(c.position() - 1);
        if (!prev.isLetterOrNumber()) {
            c.insertText(QString(typed) + typed);
            c.movePosition(QTextCursor::PreviousCharacter);
            setTextCursor(c);
            return true;
        }
        return false;
    }

    if (openers.contains(typed)) {
        if (c.hasSelection()) {
            const QString sel = c.selectedText();
            const QChar closer = closers.at(openers.indexOf(typed));
            c.insertText(typed + sel + closer);
            return true;
        }
        c.insertText(QString(typed) + closers.at(openers.indexOf(typed)));
        c.movePosition(QTextCursor::PreviousCharacter);
        setTextCursor(c);
        return true;
    }

    // Backspace between an empty pair deletes both.
    if (e->key() == Qt::Key_Backspace && !c.hasSelection()) {
        const QChar prev = document()->characterAt(c.position() - 1);
        if (openers.contains(prev) && document()->characterAt(c.position()) == closers.at(openers.indexOf(prev))) {
            c.setPosition(c.position() - 1, QTextCursor::KeepAnchor);
            c.setPosition(c.position() + 1, QTextCursor::KeepAnchor);
            c.removeSelectedText();
            return true;
        }
    }
    return false;
}

void CodeEditor::keyPressEvent(QKeyEvent* e)
{
    if (handleAutoClose(e)) return;

    if (e->key() == Qt::Key_Tab || e->key() == Qt::Key_Backtab) {
        indentSelection(e->key() == Qt::Key_Tab);
        return;
    }
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        handleAutoIndent();
        return;
    }
    QPlainTextEdit::keyPressEvent(e);
}

}  // namespace cf
