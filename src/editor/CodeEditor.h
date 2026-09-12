#pragma once
// CodeEditor: QPlainTextEdit-based editor view with line numbers, fold margin,
// current-line highlight, indent guides, bracket matching, auto-indent,
// auto-closing brackets, multi-cursor editing, bookmarks, LSP diagnostic
// squiggles, bracket pair colorization and sticky scroll. One instance per
// open document per editor group.
#include <QPlainTextEdit>
#include <QSet>
#include <QWidget>

#include "core/TextDocument.h"
#include "lsp/LanguageService.h"

class QTextBlock;

namespace cf {

class CodeEditor;
class Minimap;
class Theme;
struct Diagnostic;

// Transparent overlay pinned to the top of the viewport showing the
// enclosing scope lines (VS Code style sticky scroll, up to 3 levels).
class StickyScrollOverlay : public QWidget {
    Q_OBJECT
public:
    explicit StickyScrollOverlay(CodeEditor* editor);
    void refresh();
    QSize sizeHint() const override { return QSize(0, m_height); }

protected:
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;

private:
    CodeEditor* m_editor;
    QVector<int> m_lines;   // outermost..innermost block numbers
    int m_height = 0;
};

class LineNumberArea : public QWidget {
public:
    explicit LineNumberArea(CodeEditor* editor);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    CodeEditor* m_editor;
    friend class CodeEditor;
};

class CodeEditor : public QPlainTextEdit {
    Q_OBJECT
public:
    CodeEditor(TextDocument* doc, QWidget* parent = nullptr);

    TextDocument* textDocument() const { return m_doc; }
    Minimap* minimap() const { return m_minimap; }

    void applySettings();            // font, tabs, wrap, whitespace, cursor...
    void applyTheme(const Theme& t);
    void applyEditorConfig();        // per-document editorconfig overrides

    // Find highlighting (owned by FindReplaceBar).
    void setFindMatches(const QList<QTextCursor>& matches, int currentIndex);
    void clearFindMatches();

    // LSP diagnostics (squiggles).
    void setDiagnostics(const QVector<Diagnostic>& diags);

    void gotoLine(int line, int column = 0);   // 0-based
    void setLineHeightApplied(bool v) { m_lineHeightApplied = v; }

    // ---- multi-cursor ----
    void addCursorAt(const QPoint& viewportPos);   // Alt+Click
    void addNextOccurrence();                      // Ctrl+D
    void skipOccurrence();                         // Ctrl+K Ctrl+D
    void addCursorAbove();                         // Ctrl+Alt+Up
    void addCursorBelow();                         // Ctrl+Alt+Down
    void clearExtraCursors();
    bool hasExtraCursors() const { return !m_extraCursors.isEmpty(); }

    // ---- bookmarks ----
    void toggleBookmark();
    void nextBookmark();
    void previousBookmark();

signals:
    void cursorStats(int line, int column, int selectedChars, int selectedLines);
    void focusGained();
    void contextRequested(const QPoint& globalPos);

public slots:
    void updateExtraSelections();
    void increaseFontSize();
    void decreaseFontSize();

protected:
    void resizeEvent(QResizeEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void paintEvent(QPaintEvent* e) override;
    void focusInEvent(QFocusEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;

private slots:
    void updateMarginWidth();
    void updateMargin(const QRect& rect, int dy);
    void onCursorMoved();
    void onDocChanged();
    void rebuildBracketDepths();

private:
    friend class LineNumberArea;
    friend class Minimap;
    friend class StickyScrollOverlay;

    // margin
    void paintMargin(QPaintEvent* e);
    int marginWidth() const;
    QTextBlock blockForPos(int y) const;

    // folding
    struct FoldRange { int start; int end; };   // block numbers
    bool foldRangeForBlock(int blockNumber, FoldRange* out) const;
    void toggleFold(int blockNumber);
    void applyFoldVisibility();
    void unfoldAround(int blockNumber);

    // editing helpers
    void indentSelection(bool more);
    void handleAutoIndent();
    bool handleAutoClose(QKeyEvent* e);

    // multi-cursor internals
    QList<QTextCursor> allCursors() const;
    void applyMultiEdit(std::function<void(QList<QTextCursor>&)> op);
    void copyWithMultipleCursors(bool cut);
    void addCursorVertical(int direction);

    // selection helpers
    QList<QTextEdit::ExtraSelection> buildExtraSelections() const;
    QTextCursor currentLineSelection() const;

    TextDocument* m_doc;
    LineNumberArea* m_margin;
    Minimap* m_minimap = nullptr;
    StickyScrollOverlay* m_sticky = nullptr;

    QSet<int> m_foldedBlocks;          // collapsed start block numbers
    QList<QTextCursor> m_findMatches;
    int m_currentFind = -1;

    // multi-cursor state
    QList<QTextCursor> m_extraCursors;
    QString m_occurrenceNeedle;
    bool m_occurrenceWholeWord = false;

    // bookmarks + diagnostics
    QSet<int> m_bookmarks;
    QVector<Diagnostic> m_diagnostics;

    // bracket pair colorization cache
    QVector<QVector<QPair<int, int>>> m_bracketsPerBlock;   // blockNumber -> [(col, depth)]
    bool m_bracketColorsValid = false;
    QTimer* m_bracketTimer = nullptr;

    // colors (from theme)
    QColor m_activeLineColor, m_lineNumberColor, m_lineNumberActiveColor;
    QColor m_indentGuideColor, m_bracketMatchColor, m_findMatchColor, m_currentFindColor;
    QColor m_foldArrowColor;
    QColor m_caretColor, m_selectionColor, m_diagnosticErrorColor, m_diagnosticWarningColor;
    QColor m_diagnosticInfoColor;
    QColor m_bracketColors[3];

    bool m_showLineNumbers = true;
    bool m_showFolding = true;
    bool m_indentGuides = true;
    bool m_highlightActiveLine = true;
    bool m_bracketMatching = true;
    bool m_autoIndent = true;
    bool m_autoClose = true;
    bool m_bracketColorization = true;
    bool m_stickyScrollEnabled = true;
    int m_tabSize = 4;
    bool m_insertSpaces = true;
    bool m_lineHeightApplied = false;
};

}  // namespace cf
