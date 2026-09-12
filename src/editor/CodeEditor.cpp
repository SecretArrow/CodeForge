#include "editor/CodeEditor.h"

#include <QAbstractScrollArea>
#include <QApplication>
#include <QClipboard>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QPolygon>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSet>
#include <QTextBlock>
#include <QTextLayout>
#include <QTimer>

#include <algorithm>
#include <functional>

#include "core/Breakpoints.h"
#include "editor/Minimap.h"
#include "editor/MultiCursorOps.h"
#include "filesystem/EditorConfig.h"
#include "settings/SettingsManager.h"
#include "syntax/LanguageRegistry.h"
#include "themes/Theme.h"

namespace cf {

static const int kStickyLinePad = 2;

// ---------------- StickyScrollOverlay ----------------

StickyScrollOverlay::StickyScrollOverlay(CodeEditor* editor)
    : QWidget(editor->viewport()), m_editor(editor)
{
    setObjectName(QStringLiteral("codeforge_stickyscroll"));
    setAttribute(Qt::WA_OpaquePaintEvent);
    hide();
}

void StickyScrollOverlay::refresh()
{
    m_lines.clear();
    m_height = 0;

    if (!m_editor->m_stickyScrollEnabled || !m_editor->isVisible() || m_editor->isReadOnly()) {
        setGeometry(0, 0, m_editor->viewport()->width(), 0);
        hide();
        update();
        return;
    }

    const int lineH = m_editor->fontMetrics().height() + kStickyLinePad;
    const int first = m_editor->firstVisibleBlock().blockNumber();
    int current = first;
    int probe = first - 1;

    QVector<int> found;   // innermost first
    while (found.size() < 3 && probe >= 0) {
        const int scanFloor = qMax(0, probe - 2000);
        bool ok = false;
        for (int b = probe; b >= scanFloor; --b) {
            CodeEditor::FoldRange r;
            if (m_editor->foldRangeForBlock(b, &r) && r.start < current && r.end >= current) {
                // Don't repeat the line already shown / the first visible line.
                if (!found.contains(r.start) && r.start != first) {
                    found.append(r.start);
                    probe = b - 1;
                    current = r.start;
                    ok = true;
                } else {
                    probe = b - 1;
                    ok = false;
                }
                break;
            }
        }
        if (!ok) break;
    }

    // Paint outermost..innermost (top to bottom).
    for (int i = found.size() - 1; i >= 0; --i) m_lines.append(found.at(i));

    m_height = int(m_lines.size()) * lineH + (m_lines.isEmpty() ? 0 : 2);
    setGeometry(0, 0, m_editor->viewport()->width(), m_height);
    setVisible(m_height > 0);
    update();
}

void StickyScrollOverlay::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    const QColor bg = m_editor->palette().window().color();
    const QColor fg = m_editor->m_lineNumberActiveColor;
    p.fillRect(rect(), bg);

    const int lineH = m_editor->fontMetrics().height() + kStickyLinePad;
    int y = 0;
    for (int blockNumber : m_lines) {
        const QTextBlock b = m_editor->document()->findBlockByNumber(blockNumber);
        if (b.isValid()) {
            const QString text = m_editor->fontMetrics().elidedText(
                b.text(), Qt::ElideRight, width() - 8);
            p.setPen(fg);
            p.drawText(4, y, width() - 8, m_editor->fontMetrics().height(), Qt::AlignVCenter, text);
        }
        y += lineH;
    }
    if (m_height > 0) {
        p.setPen(bg.darker(140));
        p.drawLine(0, m_height - 1, width(), m_height - 1);
    }
}

void StickyScrollOverlay::mousePressEvent(QMouseEvent* e)
{
    const int lineH = m_editor->fontMetrics().height() + kStickyLinePad;
    const int idx = qBound(0, int(e->pos().y()) / lineH, m_lines.size() - 1);
    if (idx < 0 || idx >= m_lines.size()) return;
    const QTextBlock b = m_editor->document()->findBlockByNumber(m_lines.at(idx));
    if (!b.isValid()) return;
    QTextCursor c(b);
    m_editor->setTextCursor(c);
    const int absY = m_editor->cursorRect(c).top() + m_editor->verticalScrollBar()->value();
    m_editor->verticalScrollBar()->setValue(absY - idx * lineH);
}

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
    // Fold zone (right edge) toggles folding; the rest of the margin toggles a
    // breakpoint for that line (VS Code behaviour).
    const QTextBlock block = m_editor->blockForPos(event->pos().y());
    if (!block.isValid()) return;
    const int foldZoneStart = m_editor->marginWidth() - 14;
    if (m_editor->m_showFolding && event->pos().x() >= foldZoneStart) {
        m_editor->toggleFold(block.blockNumber());
        return;
    }
    if (!m_editor->m_doc->isUntitled())
        BreakpointStore::instance().toggle(m_editor->m_doc->filePath(), block.blockNumber());
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
    m_bracketTimer = new QTimer(this);
    m_bracketTimer->setSingleShot(true);
    m_bracketTimer->setInterval(150);
    connect(m_bracketTimer, &QTimer::timeout, this, &CodeEditor::rebuildBracketDepths);

    m_sticky = new StickyScrollOverlay(this);

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
    rebuildBracketDepths();
    refreshBreakpoints();
}

void CodeEditor::refreshBreakpoints()
{
    if (!m_doc || m_doc->isUntitled()) return;
    if (!m_breakpointsWired) {
        m_breakpointsWired = true;
        connect(&BreakpointStore::instance(), &BreakpointStore::changed, this,
                &CodeEditor::refreshBreakpoints, Qt::QueuedConnection);
    }
    const QSet<int> lines = BreakpointStore::instance().linesFor(m_doc->filePath());
    if (lines != m_breakpoints) {
        m_breakpoints = lines;
        m_margin->update();
    }
}

void CodeEditor::addKeyInterceptor(EditorKeyInterceptor* i)
{
    if (i && !m_interceptors.contains(i)) m_interceptors.append(i);
}

void CodeEditor::removeKeyInterceptor(EditorKeyInterceptor* i)
{
    m_interceptors.removeAll(i);
}

void CodeEditor::setInlineSuggestion(const QString& text)
{
    if (m_inlineSuggestion == text) return;
    m_inlineSuggestion = text;
    viewport()->update();
}

void CodeEditor::acceptInlineSuggestion()
{
    if (m_inlineSuggestion.isEmpty() || isReadOnly()) return;
    QTextCursor c = textCursor();
    c.insertText(m_inlineSuggestion);
    m_inlineSuggestion.clear();
    setTextCursor(c);
    viewport()->update();
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
    m_bracketColorization = s.getBool(QStringLiteral("editor.bracketPairColorization"));
    m_stickyScrollEnabled = s.getBool(QStringLiteral("editor.stickyScroll"));
    setReadOnly(m_doc->isReadOnly());

    setViewportMargins(marginWidth(), 0, 0, 0);
    updateExtraSelections();
    m_margin->update();
    if (m_sticky) m_sticky->refresh();
    m_bracketTimer->start(150);

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

void CodeEditor::applyEditorConfig()
{
    const EditorConfigProps& props = m_doc->editorConfig();
    if (!props.valid) return;
    if (!props.indentStyle.isEmpty())
        m_insertSpaces = (props.indentStyle == QLatin1String("space"));
    if (props.indentSize > 0)
        m_tabSize = props.indentSize;
    else if (props.tabWidth > 0)
        m_tabSize = props.tabWidth;
    setTabStopDistance(fontMetrics().horizontalAdvance(QLatin1Char(' ')) * m_tabSize);
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
    m_caretColor = t.color(QStringLiteral("editor.caret"), t.editorForeground());
    m_selectionColor = t.color(QStringLiteral("editor.selection"));
    m_selectionColor.setAlpha(150);
    m_diagnosticErrorColor = t.color(QStringLiteral("editor.diagnosticError"), QColor(0xf1, 0x4c, 0x4c));
    m_diagnosticWarningColor = t.color(QStringLiteral("editor.diagnosticWarning"), QColor(0xcc, 0xa7, 0x00));
    m_diagnosticInfoColor = t.color(QStringLiteral("editor.diagnosticInfo"), QColor(0x37, 0x94, 0xff));
    m_bracketColors[0] = t.color(QStringLiteral("editor.bracketColor1"), QColor(0xff, 0xd7, 0x00));
    m_bracketColors[1] = t.color(QStringLiteral("editor.bracketColor2"), QColor(0x4e, 0xc9, 0xb0));
    m_bracketColors[2] = t.color(QStringLiteral("editor.bracketColor3"), QColor(0xc5, 0x86, 0xc0));
    for (QColor& c : m_bracketColors) c.setAlpha(52);
    m_breakpointColor = t.color(QStringLiteral("editor.breakpoint"), QColor(0xe5, 0x14, 0x00));
    m_ghostColor = t.color(QStringLiteral("editor.inlineSuggestion"), QColor(0x88, 0x88, 0x88));
    m_ghostColor.setAlpha(190);

    QPalette pal = viewport()->palette();
    pal.setColor(QPalette::Base, t.editorBackground());
    pal.setColor(QPalette::Text, t.editorForeground());
    pal.setColor(QPalette::Highlight, t.color(QStringLiteral("editor.selection")));
    pal.setColor(QPalette::HighlightedText, t.editorForeground());
    viewport()->setPalette(pal);
    setPalette(pal);

    updateExtraSelections();
    m_margin->update();
    if (m_sticky) m_sticky->refresh();
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
    return qMax(w, 12);   // room for bookmarks even in minimal mode
}

void CodeEditor::updateMargin(const QRect& rect, int dy)
{
    if (dy) m_margin->scroll(0, dy);
    else m_margin->update(0, rect.y(), m_margin->width(), rect.height());
    if (rect.contains(viewport()->rect())) updateMarginWidth();
    if (m_sticky && m_sticky->isVisible()) m_sticky->refresh();
}

void CodeEditor::resizeEvent(QResizeEvent* e)
{
    QPlainTextEdit::resizeEvent(e);
    QRect cr = contentsRect();
    m_margin->setGeometry(cr.left(), cr.top(), marginWidth(), cr.height());
    if (m_minimap) m_minimap->syncViewport();
    if (m_sticky) m_sticky->refresh();
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
        if (block.isVisible() && m_breakpoints.contains(blockNumber)) {
            // Breakpoint dot at the left edge.
            p.save();
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(Qt::NoPen);
            p.setBrush(m_breakpointColor);
            p.drawEllipse(QPoint(6, top + fm.height() / 2), 4, 4);
            p.restore();
        }
        if (block.isVisible() && m_bookmarks.contains(blockNumber)) {
            // Bookmark glyph (shifted right when a breakpoint dot is shown).
            p.save();
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(Qt::NoPen);
            p.setBrush(m_diagnosticInfoColor);
            const int mx = m_breakpoints.contains(blockNumber) ? 12 : 2;
            const int my = top + 2;
            const int mw = 6;
            const int mh = qMax(4, fm.height() - 4);
            QPolygon bookmark;
            bookmark << QPoint(mx, my) << QPoint(mx + mw, my) << QPoint(mx + mw, my + mh)
                     << QPoint(mx + mw / 2, my + mh - 3) << QPoint(mx, my + mh);
            p.drawPolygon(bookmark);
            p.restore();
        }
        if (block.isVisible() && m_showLineNumbers) {
            p.setPen(blockNumber == currentLine ? m_lineNumberActiveColor : m_lineNumberColor);
            p.drawText(9, top, m_margin->width() - 20, fm.height(),
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

// ---------------- bracket pair colorization ----------------

void CodeEditor::rebuildBracketDepths()
{
    m_bracketTimer->stop();
    m_bracketColorsValid = false;
    m_bracketsPerBlock.clear();

    if (m_bracketColorization && document()->characterCount() <= 400000) {
        const int blocks = document()->blockCount();
        m_bracketsPerBlock.resize(blocks);
        int depth = 0;
        bool inBlockComment = false;
        int bn = 0;
        for (QTextBlock b = document()->firstBlock(); b.isValid(); b = b.next(), ++bn) {
            const QString text = b.text();
            QVector<QPair<int, int>>& list = m_bracketsPerBlock[bn];
            bool inLineComment = false;
            QChar stringChar;
            for (int i = 0; i < text.size(); ++i) {
                const QChar ch = text.at(i);
                if (inBlockComment) {
                    if (ch == u'/' && i > 0 && text.at(i - 1) == u'*') inBlockComment = false;
                    continue;
                }
                if (inLineComment) break;
                if (ch == u'"' || ch == u'\'' || ch == u'`') {
                    if (stringChar.isNull())
                        stringChar = ch;
                    else if (stringChar == ch && (i == 0 || text.at(i - 1) != u'\\'))
                        stringChar = QChar();
                    continue;
                }
                if (!stringChar.isNull()) continue;
                if (ch == u'/' && i + 1 < text.size()) {
                    const QChar next = text.at(i + 1);
                    if (next == u'/') { inLineComment = true; continue; }
                    if (next == u'*') { inBlockComment = true; continue; }
                }
                if (ch == u'{' || ch == u'[' || ch == u'(') {
                    list.append({i, depth});
                    ++depth;
                } else if (ch == u'}' || ch == u']' || ch == u')') {
                    if (depth > 0) { --depth; list.append({i, depth}); }
                }
            }
        }
        m_bracketColorsValid = true;
    }
    updateExtraSelections();
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

    // Bracket pair colorization (visible area only).
    if (m_bracketColorsValid) {
        QTextBlock b = firstVisibleBlock();
        int bn = b.blockNumber();
        const int viewH = viewport()->height();
        while (b.isValid() && bn < m_bracketsPerBlock.size()) {
            const QRect geom = blockBoundingGeometry(b).translated(contentOffset()).toRect();
            if (geom.top() > viewH) break;
            if (b.isVisible()) {
                for (const auto& pr : m_bracketsPerBlock.at(bn)) {
                    QTextEdit::ExtraSelection es;
                    es.format.setBackground(m_bracketColors[pr.second % 3]);
                    es.cursor = QTextCursor(document());
                    es.cursor.setPosition(b.position() + pr.first);
                    es.cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
                    sel.append(es);
                }
            }
            b = b.next();
            ++bn;
        }
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

    // Diagnostic squiggles (LSP).
    for (const Diagnostic& d : m_diagnostics) {
        const QTextBlock b = document()->findBlockByNumber(d.line);
        if (!b.isValid()) continue;
        const int maxCol = qMax(0, b.length() - 1);
        const int startCol = qBound(0, d.column, maxCol);
        int len = d.length > 0 ? d.length : qMax(1, b.text().mid(startCol).trimmed().size());
        len = qBound(1, len, maxCol - startCol + 1);
        QTextEdit::ExtraSelection es;
        es.cursor = QTextCursor(document());
        es.cursor.setPosition(b.position() + startCol);
        es.cursor.setPosition(qMin(b.position() + startCol + len, b.position() + maxCol),
                              QTextCursor::KeepAnchor);
        es.format.setUnderlineStyle(QTextCharFormat::WaveUnderline);
        es.format.setUnderlineColor(d.severity == Diagnostic::Error ? m_diagnosticErrorColor
                                    : d.severity == Diagnostic::Warning ? m_diagnosticWarningColor
                                                                        : m_diagnosticInfoColor);
        sel.append(es);
    }

    // Find matches.
    for (int i = 0; i < m_findMatches.size(); ++i) {
        QTextEdit::ExtraSelection es;
        es.format.setBackground(i == m_currentFind ? m_currentFindColor : m_findMatchColor);
        es.cursor = m_findMatches.at(i);
        sel.append(es);
    }

    // Extra cursor selections.
    for (const QTextCursor& c : m_extraCursors) {
        if (!c.hasSelection()) continue;
        QTextEdit::ExtraSelection es;
        es.format.setBackground(m_selectionColor);
        es.cursor = c;
        sel.append(es);
    }
    return sel;
}

void CodeEditor::updateExtraSelections()
{
    setExtraSelections(buildExtraSelections());
}

void CodeEditor::setDiagnostics(const QVector<Diagnostic>& diags)
{
    m_diagnostics = diags;
    updateExtraSelections();
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
    m_bracketTimer->start(150);
    if (m_sticky) m_sticky->refresh();
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

// ---------------- multi-cursor ----------------

QList<QTextCursor> CodeEditor::allCursors() const
{
    QList<QTextCursor> cursors;
    cursors.append(textCursor());
    cursors.append(m_extraCursors);
    return cursors;
}

void CodeEditor::applyMultiEdit(std::function<void(QList<QTextCursor>&)> op)
{
    QList<QTextCursor> cursors = allCursors();
    multicursor::normalize(cursors);
    if (cursors.isEmpty()) return;
    op(cursors);
    multicursor::normalize(cursors);
    if (!cursors.isEmpty()) {
        setTextCursor(cursors.first());
        m_extraCursors = cursors.mid(1);
    }
    updateExtraSelections();
    viewport()->update();
}

void CodeEditor::copyWithMultipleCursors(bool cut)
{
    QList<QTextCursor> cursors = allCursors();
    multicursor::normalize(cursors);
    QStringList texts;
    for (const QTextCursor& c : cursors) {
        if (c.hasSelection())
            texts << c.selectedText().replace(QChar(u'\u2029'), QLatin1Char('\n'));
    }
    if (texts.isEmpty()) return;
    QGuiApplication::clipboard()->setText(texts.join(QLatin1Char('\n')));
    if (cut) applyMultiEdit([](QList<QTextCursor>& cs) { multicursor::removeSelections(cs); });
}

void CodeEditor::addCursorAt(const QPoint& viewportPos)
{
    const QTextCursor c = cursorForPosition(viewportPos);
    if (!c.block().isValid()) return;
    // Clicking an existing extra cursor removes it (VS Code behavior).
    for (int i = 0; i < m_extraCursors.size(); ++i) {
        if (m_extraCursors.at(i).position() == c.position()) {
            m_extraCursors.removeAt(i);
            updateExtraSelections();
            viewport()->update();
            return;
        }
    }
    m_extraCursors.append(c);
    updateExtraSelections();
    viewport()->update();
}

void CodeEditor::addNextOccurrence()
{
    QTextCursor c = textCursor();
    QString needle;
    if (c.hasSelection()) {
        needle = c.selectedText().replace(QChar(u'\u2029'), QLatin1Char('\n'));
        m_occurrenceWholeWord = false;
    } else {
        const auto r = multicursor::wordRangeAt(document(), c.position());
        if (r.first < 0) return;
        needle = document()->toPlainText().mid(r.first, r.second - r.first);
        m_occurrenceWholeWord = true;
    }
    if (needle.isEmpty()) return;

    if (needle != m_occurrenceNeedle) {
        // Fresh occurrence chain: select at primary, remember needle.
        m_occurrenceNeedle = needle;
        if (!c.hasSelection()) {
            const auto r = multicursor::wordRangeAt(document(), c.position());
            c.setPosition(r.first);
            c.setPosition(r.second, QTextCursor::KeepAnchor);
            setTextCursor(c);
        }
        return;
    }

    // Find next occurrence after the furthest selection.
    int lastEnd = 0;
    for (const QTextCursor& cur : allCursors())
        lastEnd = qMax(lastEnd, cur.selectionEnd());
    QList<QPair<int, int>> taken;
    for (const QTextCursor& cur : allCursors())
        if (cur.hasSelection()) taken.append({cur.selectionStart(), cur.selectionEnd()});
    const int pos = multicursor::nextOccurrence(document(), needle, lastEnd, m_occurrenceWholeWord, taken);
    if (pos < 0) return;
    QTextCursor nc(document());
    nc.setPosition(pos);
    nc.setPosition(pos + needle.size(), QTextCursor::KeepAnchor);
    m_extraCursors.append(nc);
    updateExtraSelections();
    viewport()->update();
}

void CodeEditor::skipOccurrence()
{
    if (m_extraCursors.isEmpty() || m_occurrenceNeedle.isEmpty()) return;
    // Replace the last added occurrence with the next one.
    QTextCursor last = m_extraCursors.takeLast();
    QList<QPair<int, int>> taken;
    for (const QTextCursor& cur : allCursors())
        if (cur.hasSelection()) taken.append({cur.selectionStart(), cur.selectionEnd()});
    const int pos = multicursor::nextOccurrence(document(), m_occurrenceNeedle, last.selectionEnd(),
                                                m_occurrenceWholeWord, taken);
    if (pos < 0) {
        m_extraCursors.append(last);
        return;
    }
    QTextCursor nc(document());
    nc.setPosition(pos);
    nc.setPosition(pos + m_occurrenceNeedle.size(), QTextCursor::KeepAnchor);
    m_extraCursors.append(nc);
    updateExtraSelections();
    viewport()->update();
}

void CodeEditor::addCursorAbove() { addCursorVertical(-1); }
void CodeEditor::addCursorBelow() { addCursorVertical(1); }

void CodeEditor::addCursorVertical(int direction)
{
    const int column = textCursor().positionInBlock();
    int minBlock = textCursor().blockNumber();
    int maxBlock = minBlock;
    for (const QTextCursor& c : m_extraCursors) {
        minBlock = qMin(minBlock, c.blockNumber());
        maxBlock = qMax(maxBlock, c.blockNumber());
    }
    const int target = direction < 0 ? minBlock - 1 : maxBlock + 1;
    const QTextBlock b = document()->findBlockByNumber(target);
    if (!b.isValid()) return;
    QTextCursor nc(b);
    nc.movePosition(QTextCursor::StartOfBlock);
    nc.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, qMin(column, qMax(0, b.length() - 1)));
    m_extraCursors.append(nc);
    updateExtraSelections();
    viewport()->update();
}

void CodeEditor::clearExtraCursors()
{
    if (m_extraCursors.isEmpty()) return;
    m_extraCursors.clear();
    m_occurrenceNeedle.clear();
    updateExtraSelections();
    viewport()->update();
}

void CodeEditor::toggleBookmark()
{
    const int block = textCursor().blockNumber();
    if (m_bookmarks.contains(block)) m_bookmarks.remove(block);
    else m_bookmarks.insert(block);
    m_margin->update();
}

void CodeEditor::nextBookmark()
{
    if (m_bookmarks.isEmpty()) return;
    QList<int> lines = m_bookmarks.values();
    std::sort(lines.begin(), lines.end());
    const int current = textCursor().blockNumber();
    for (int l : lines) {
        if (l > current) { gotoLine(l); return; }
    }
    gotoLine(lines.first());
}

void CodeEditor::previousBookmark()
{
    if (m_bookmarks.isEmpty()) return;
    QList<int> lines = m_bookmarks.values();
    std::sort(lines.begin(), lines.end(), std::greater<int>());
    const int current = textCursor().blockNumber();
    for (int l : lines) {
        if (l < current) { gotoLine(l); return; }
    }
    gotoLine(lines.first());
}

// ---------------- editing ----------------

void CodeEditor::indentSelection(bool more)
{
    QList<QTextCursor> cursors;
    if (!m_extraCursors.isEmpty())
        cursors = allCursors();
    else
        cursors.append(textCursor());
    multicursor::indent(cursors, m_tabSize, m_insertSpaces, more);
    if (!cursors.isEmpty()) setTextCursor(cursors.first());
    updateExtraSelections();
    viewport()->update();
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
    // Registered interceptors (completion popup, Vim/Emacs, inline AI) get the
    // event first; the most recently registered wins.
    for (int i = m_interceptors.size() - 1; i >= 0; --i) {
        if (m_interceptors.at(i)->editorKeyPress(this, e)) {
            e->accept();
            return;
        }
    }

    // Multi-cursor editing takes precedence when extra cursors exist.
    if (!m_extraCursors.isEmpty() && !isReadOnly()) {
        if (e->key() == Qt::Key_Escape) {
            clearExtraCursors();
            e->accept();
            return;
        }
        if (e->matches(QKeySequence::Copy)) { copyWithMultipleCursors(false); e->accept(); return; }
        if (e->matches(QKeySequence::Cut))  { copyWithMultipleCursors(true);  e->accept(); return; }

        switch (e->key()) {
        case Qt::Key_Backspace:
            applyMultiEdit([this](QList<QTextCursor>& cs) { multicursor::backspace(cs, m_tabSize, m_insertSpaces); });
            e->accept();
            return;
        case Qt::Key_Delete:
            applyMultiEdit([](QList<QTextCursor>& cs) { multicursor::deleteForward(cs); });
            e->accept();
            return;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            applyMultiEdit([](QList<QTextCursor>& cs) { multicursor::newline(cs); });
            e->accept();
            return;
        case Qt::Key_Tab:
            applyMultiEdit([this](QList<QTextCursor>& cs) { multicursor::indent(cs, m_tabSize, m_insertSpaces, true); });
            e->accept();
            return;
        case Qt::Key_Backtab:
            applyMultiEdit([this](QList<QTextCursor>& cs) { multicursor::indent(cs, m_tabSize, m_insertSpaces, false); });
            e->accept();
            return;
        default:
            break;
        }

        const QString t = e->text();
        if (t.size() == 1 && t.at(0).isPrint()) {
            applyMultiEdit([&t](QList<QTextCursor>& cs) { multicursor::insertText(cs, t); });
            e->accept();
            return;
        }

        // Navigation keys collapse back to a single cursor.
        switch (e->key()) {
        case Qt::Key_Left: case Qt::Key_Right: case Qt::Key_Up: case Qt::Key_Down:
        case Qt::Key_Home: case Qt::Key_End: case Qt::Key_PageUp: case Qt::Key_PageDown:
            clearExtraCursors();
            break;
        default:
            break;
        }
    }

    if (e->key() == Qt::Key_Escape) {
        clearExtraCursors();
        m_occurrenceNeedle.clear();
        if (!m_inlineSuggestion.isEmpty()) {
            m_inlineSuggestion.clear();
            viewport()->update();
        }
    }

    if (handleAutoClose(e)) return;

    if (e->key() == Qt::Key_Tab && !m_inlineSuggestion.isEmpty()) {
        acceptInlineSuggestion();
        return;
    }
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

void CodeEditor::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton && (e->modifiers() & Qt::AltModifier) && !isReadOnly()) {
        addCursorAt(e->pos());
        e->accept();
        return;
    }
    QPlainTextEdit::mousePressEvent(e);
}

void CodeEditor::paintEvent(QPaintEvent* e)
{
    QPlainTextEdit::paintEvent(e);
    QPainter p(viewport());
    if (!m_extraCursors.isEmpty()) {
        const int w = qMax(1, cursorWidth());
        for (const QTextCursor& c : m_extraCursors) {
            const QRect r = cursorRect(c);
            if (r.bottom() < -50 || r.top() > height() + 50) continue;
            p.fillRect(r.x(), r.y(), w, r.height(), m_caretColor);
        }
    }
    // AI inline suggestion ghost text right after the caret.
    if (!m_inlineSuggestion.isEmpty() && m_extraCursors.isEmpty()) {
        const QRect r = cursorRect();
        if (r.isValid() && r.top() < height() && r.bottom() > 0) {
            p.setPen(m_ghostColor);
            p.drawText(r.left(), r.top() + fontMetrics().ascent(), m_inlineSuggestion);
        }
    }
}

}  // namespace cf
