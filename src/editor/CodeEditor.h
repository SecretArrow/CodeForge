#pragma once
// CodeEditor: QPlainTextEdit-based editor view with line numbers, fold margin,
// current-line highlight, indent guides, bracket matching, auto-indent and
// auto-closing brackets. One instance per open document per editor group.
#include <QPlainTextEdit>
#include <QWidget>

#include "core/TextDocument.h"

class QTextBlock;

namespace cf {

class CodeEditor;
class Minimap;
class Theme;

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

    // Find highlighting (owned by FindReplaceBar).
    void setFindMatches(const QList<QTextCursor>& matches, int currentIndex);
    void clearFindMatches();

    void gotoLine(int line, int column = 0);   // 0-based
    void setLineHeightApplied(bool v) { m_lineHeightApplied = v; }

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
    void focusInEvent(QFocusEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;

private slots:
    void updateMarginWidth();
    void updateMargin(const QRect& rect, int dy);
    void onCursorMoved();
    void onDocChanged();

private:
    friend class LineNumberArea;
    friend class Minimap;

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

    // selection helpers
    QList<QTextEdit::ExtraSelection> buildExtraSelections() const;
    QTextCursor currentLineSelection() const;

    TextDocument* m_doc;
    LineNumberArea* m_margin;
    Minimap* m_minimap = nullptr;

    QSet<int> m_foldedBlocks;          // collapsed start block numbers
    QList<QTextCursor> m_findMatches;
    int m_currentFind = -1;

    // colors (from theme)
    QColor m_activeLineColor, m_lineNumberColor, m_lineNumberActiveColor;
    QColor m_indentGuideColor, m_bracketMatchColor, m_findMatchColor, m_currentFindColor;
    QColor m_foldArrowColor;

    bool m_showLineNumbers = true;
    bool m_showFolding = true;
    bool m_indentGuides = true;
    bool m_highlightActiveLine = true;
    bool m_bracketMatching = true;
    bool m_autoIndent = true;
    bool m_autoClose = true;
    int m_tabSize = 4;
    bool m_insertSpaces = true;
    bool m_lineHeightApplied = false;
};

}  // namespace cf
