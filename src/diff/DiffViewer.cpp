#include "diff/DiffViewer.h"

#include <QAbstractSlider>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QVBoxLayout>

#include "diff/DiffEngine.h"
#include "themes/ThemeManager.h"

namespace cf {

DiffViewer::DiffViewer(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    auto* toolbar = new QHBoxLayout;
    toolbar->setContentsMargins(6, 4, 6, 4);
    m_titleA = new QLineEdit(this);
    m_titleA->setReadOnly(true);
    m_titleB = new QLineEdit(this);
    m_titleB->setReadOnly(true);
    auto* copyLeft = new QPushButton(tr("< Copy"), this);
    copyLeft->setToolTip(tr("Copy the current hunk from the right side to the left side"));
    auto* copyRight = new QPushButton(tr("Copy >"), this);
    copyRight->setToolTip(tr("Copy the current hunk from the left side to the right side"));
    auto* prev = new QPushButton(tr("Prev"), this);
    auto* next = new QPushButton(tr("Next"), this);
    m_status = new QLabel(this);
    toolbar->addWidget(new QLabel(tr("A:"), this));
    toolbar->addWidget(m_titleA, 1);
    toolbar->addWidget(new QLabel(tr("B:"), this));
    toolbar->addWidget(m_titleB, 1);
    toolbar->addWidget(copyLeft);
    toolbar->addWidget(copyRight);
    toolbar->addWidget(prev);
    toolbar->addWidget(next);
    toolbar->addWidget(m_status);
    layout->addLayout(toolbar);

    auto* split = new QHBoxLayout;
    split->setContentsMargins(0, 0, 0, 0);
    split->setSpacing(2);
    m_editA = new QPlainTextEdit(this);
    m_editB = new QPlainTextEdit(this);
    for (QPlainTextEdit* e : { m_editA, m_editB }) {
        e->setReadOnly(true);
        e->setLineWrapMode(QPlainTextEdit::NoWrap);
        QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        mono.setPointSize(11);
        e->setFont(mono);
        e->setFrameShape(QFrame::NoFrame);
    }
    split->addWidget(m_editA, 1);
    split->addWidget(m_editB, 1);
    layout->addLayout(split, 1);

    connect(m_editA->verticalScrollBar(), &QScrollBar::valueChanged, this,
            &DiffViewer::onScrollMoved);
    connect(m_editB->verticalScrollBar(), &QScrollBar::valueChanged, this,
            &DiffViewer::onScrollMoved);
    connect(copyRight, &QPushButton::clicked, this, &DiffViewer::onCopyRight);
    connect(copyLeft, &QPushButton::clicked, this, &DiffViewer::onCopyLeft);
    connect(prev, &QPushButton::clicked, this, &DiffViewer::previousHunk);
    connect(next, &QPushButton::clicked, this, &DiffViewer::nextHunk);

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this,
            [this](const Theme&) { colorizeRows(); });
    colorizeRows();
}

void DiffViewer::setContents(const QString& titleA, const QString& textA,
                             const QString& titleB, const QString& textB)
{
    m_textA = textA;
    m_textB = textB;
    m_titleA->setText(titleA);
    m_titleB->setText(titleB);
    m_currentHunk = -1;
    rebuild();
}

void DiffViewer::rebuild()
{
    m_computing = true;
    DiffEngine engine;
    engine.compute(m_textA, m_textB);

    // Both sides display the padded row texts so corresponding lines align.
    QString left, right;
    m_rowTypes.clear();
    m_hunkRowStarts.clear();
    bool inHunk = false;
    for (const DiffRow& r : engine.rows()) {
        if (r.type != DiffRow::Type::Equal && !inHunk) {
            m_hunkRowStarts.append(static_cast<int>(m_rowTypes.size()));
            inHunk = true;
        }
        if (r.type == DiffRow::Type::Equal)
            inHunk = false;
        m_rowTypes.append(static_cast<int>(r.type));
        left += (r.type == DiffRow::Type::Add ? QString() : r.left) + QLatin1Char('\n');
        right += (r.type == DiffRow::Type::Delete ? QString() : r.right) + QLatin1Char('\n');
    }
    m_editA->setPlainText(left);
    m_editB->setPlainText(right);
    colorizeRows();

    const QString extra = engine.usedFallback() ? tr("  (large diff, approximate)") : QString();
    m_status->setText(tr("%1 hunk(s)%2").arg(engine.hunks().size()).arg(extra));
    m_computing = false;
}

void DiffViewer::colorizeRows()
{
    const Theme& t = ThemeManager::instance().currentTheme();
    const QColor addBg = t.color(QStringLiteral("diff.addBackground"), QColor(46, 160, 67, 42));
    const QColor delBg = t.color(QStringLiteral("diff.deleteBackground"), QColor(248, 81, 73, 42));
    const QColor hunkBg = t.color(QStringLiteral("diff.hunkBackground"), QColor(88, 166, 255, 26));

    auto mark = [addBg, delBg, hunkBg, this](QPlainTextEdit* edit, bool leftSide) {
        QList<QTextEdit::ExtraSelection> sels;
        QTextBlock b = edit->document()->firstBlock();
        int row = 0;
        while (b.isValid() && row < m_rowTypes.size()) {
            const DiffRow::Type ty = static_cast<DiffRow::Type>(m_rowTypes.at(row));
            QColor bg;
            if (ty == DiffRow::Type::Add && !leftSide)
                bg = addBg;
            else if (ty == DiffRow::Type::Delete && leftSide)
                bg = delBg;
            if (!m_hunkRowStarts.isEmpty() && m_currentHunk >= 0
                && m_currentHunk < m_hunkRowStarts.size()) {
                // rows of the current hunk: from its start row up to the next
                // hunk start or end of list (rows are contiguous non-equal)
                const int start = m_hunkRowStarts.at(m_currentHunk);
                const int end = m_currentHunk + 1 < m_hunkRowStarts.size()
                                    ? m_hunkRowStarts.at(m_currentHunk + 1)
                                    : m_rowTypes.size();
                if (row >= start && row < end && ty != DiffRow::Type::Equal)
                    bg = bg.isValid() ? bg : hunkBg;
            }
            if (bg.isValid()) {
                QTextEdit::ExtraSelection s;
                s.format.setBackground(bg);
                s.cursor = QTextCursor(b);
                s.cursor.clearSelection();
                sels.append(s);
            }
            b = b.next();
            ++row;
        }
        edit->setExtraSelections(sels);
    };
    mark(m_editA, true);
    mark(m_editB, false);
}

void DiffViewer::onScrollMoved(int value)
{
    if (m_syncing || m_computing)
        return;
    m_syncing = true;
    if (sender() == m_editA->verticalScrollBar())
        m_editB->verticalScrollBar()->setValue(value);
    else
        m_editA->verticalScrollBar()->setValue(value);
    m_syncing = false;
}

void DiffViewer::highlightCurrentHunk()
{
    if (m_hunkRowStarts.isEmpty()) {
        m_status->setText(tr("No differences"));
        colorizeRows();
        return;
    }
    m_currentHunk = qBound(0, m_currentHunk, m_hunkRowStarts.size() - 1);
    const int block = m_hunkRowStarts.at(m_currentHunk);
    m_status->setText(tr("Hunk %1 of %2").arg(m_currentHunk + 1).arg(m_hunkRowStarts.size()));
    QTextBlock bA = m_editA->document()->findBlockByNumber(block);
    if (bA.isValid()) {
        m_editA->setTextCursor(QTextCursor(bA));
        m_editA->centerCursor();
    }
    QTextBlock bB = m_editB->document()->findBlockByNumber(block);
    if (bB.isValid()) {
        m_editB->setTextCursor(QTextCursor(bB));
        m_editB->centerCursor();
    }
    colorizeRows();
}

void DiffViewer::nextHunk()
{
    if (m_hunkRowStarts.isEmpty())
        return;
    m_currentHunk = m_currentHunk + 1 >= m_hunkRowStarts.size() ? 0 : m_currentHunk + 1;
    highlightCurrentHunk();
}

void DiffViewer::previousHunk()
{
    if (m_hunkRowStarts.isEmpty())
        return;
    m_currentHunk = m_currentHunk - 1 < 0 ? m_hunkRowStarts.size() - 1 : m_currentHunk - 1;
    highlightCurrentHunk();
}

QString DiffViewer::currentSideText(bool right) const
{
    return right ? m_textB : m_textA;
}

void DiffViewer::setSideText(bool right, const QString& text)
{
    if (right)
        m_textB = text;
    else
        m_textA = text;
}

void DiffViewer::applyHunk(bool toRight, int hunkIndex)
{
    DiffEngine engine;
    engine.compute(m_textA, m_textB);
    if (hunkIndex < 0 || hunkIndex >= engine.hunks().size())
        return;
    const DiffHunk h = engine.hunks().at(hunkIndex);

    // Copying A -> B: B loses its Add rows and gains A's Delete rows.
    // Copying B -> A is the mirror image.
    QStringList replacement;
    int startLine = -1;
    int removeCount = 0;
    for (int r = h.rowStart; r < h.rowStart + h.rowCount; ++r) {
        const DiffRow& row = engine.rows().at(r);
        if (toRight) {
            if (row.type == DiffRow::Type::Delete)
                replacement.append(row.left);
            else if (row.type == DiffRow::Type::Add) {
                if (startLine < 0)
                    startLine = row.bLine;
                ++removeCount;
            }
        } else {
            if (row.type == DiffRow::Type::Add)
                replacement.append(row.right);
            else if (row.type == DiffRow::Type::Delete) {
                if (startLine < 0)
                    startLine = row.aLine;
                ++removeCount;
            }
        }
    }

    QString target = toRight ? m_textB : m_textA;
    QStringList tgtLines = DiffEngine::splitLines(target);
    if (startLine < 0)
        startLine = tgtLines.size();
    startLine = qBound(0, startLine, tgtLines.size());
    removeCount = qBound(0, removeCount, tgtLines.size() - startLine);
    for (int i = 0; i < removeCount; ++i)
        tgtLines.removeAt(startLine);
    for (int i = 0; i < replacement.size(); ++i)
        tgtLines.insert(startLine + i, replacement.at(i));

    setSideText(toRight, tgtLines.join(QLatin1Char('\n')));
    rebuild();
    emit contentsEdited();
}

void DiffViewer::copyHunkToRight(int hunkIndex)
{
    applyHunk(true, hunkIndex);
}

void DiffViewer::copyHunkToLeft(int hunkIndex)
{
    applyHunk(false, hunkIndex);
}

void DiffViewer::onCopyRight()
{
    if (m_hunkRowStarts.isEmpty())
        return;
    if (m_currentHunk < 0)
        m_currentHunk = 0;
    applyHunk(true, m_currentHunk);
    highlightCurrentHunk();
}

void DiffViewer::onCopyLeft()
{
    if (m_hunkRowStarts.isEmpty())
        return;
    if (m_currentHunk < 0)
        m_currentHunk = 0;
    applyHunk(false, m_currentHunk);
    highlightCurrentHunk();
}

}  // namespace cf
