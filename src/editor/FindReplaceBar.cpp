#include "editor/FindReplaceBar.h"

#include <QCheckBox>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QTextCursor>
#include <QTextDocument>
#include <QToolButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include "editor/CodeEditor.h"
#include "ui/Icons.h"

namespace cf {

FindReplaceBar::FindReplaceBar(CodeEditor* editor, QWidget* parent)
    : QWidget(parent), m_editor(editor)
{
    setObjectName(QStringLiteral("codeforge_findbar"));

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(4);

    m_findEdit = new QLineEdit(this);
    m_findEdit->setPlaceholderText(tr("Find"));
    m_findEdit->setClearButtonEnabled(true);
    m_findEdit->setFixedWidth(240);
    layout->addWidget(m_findEdit);

    auto mkToggle = [this](const QString& tip) {
        auto* b = new QToolButton(this);
        b->setCheckable(true);
        b->setToolTip(tip);
        b->setFixedSize(24, 22);
        return b;
    };
    m_caseBtn = mkToggle(tr("Match case (Aa)"));
    m_caseBtn->setText(QStringLiteral("Aa"));
    m_wordBtn = mkToggle(tr("Whole word (ab|)"));
    m_wordBtn->setText(QStringLiteral("ab|"));
    m_regexBtn = mkToggle(tr("Regular expression (.*)"));
    m_regexBtn->setText(QStringLiteral(".*"));
    layout->addWidget(m_caseBtn);
    layout->addWidget(m_wordBtn);
    layout->addWidget(m_regexBtn);

    auto* countLabel = new QLabel(this);
    countLabel->setObjectName(QStringLiteral("find_count"));
    layout->addWidget(countLabel);

    m_prevBtn = new QToolButton(this);
    m_prevBtn->setIcon(Icons::icon(Icons::Name::ChevronDown));
    m_prevBtn->setToolTip(tr("Previous match (Shift+Enter)"));
    m_nextBtn = new QToolButton(this);
    m_nextBtn->setIcon(Icons::icon(Icons::Name::ChevronRight));
    m_nextBtn->setToolTip(tr("Next match (Enter)"));
    layout->addWidget(m_prevBtn);
    layout->addWidget(m_nextBtn);

    auto* closeBtn = new QToolButton(this);
    closeBtn->setIcon(Icons::icon(Icons::Name::Close));
    closeBtn->setToolTip(tr("Close (Escape)"));
    layout->addWidget(closeBtn);

    m_replaceToggle = new QPushButton(tr("Replace"), this);
    m_replaceToggle->setFlat(true);
    layout->addWidget(m_replaceToggle);

    m_replaceEdit = new QLineEdit(this);
    m_replaceEdit->setPlaceholderText(tr("Replace with"));
    m_replaceEdit->setFixedWidth(240);
    m_replaceEdit->hide();
    layout->addWidget(m_replaceEdit);

    m_replaceOneBtn = new QPushButton(tr("Replace"), this);
    m_replaceOneBtn->hide();
    layout->addWidget(m_replaceOneBtn);

    m_replaceAllBtn = new QPushButton(tr("All"), this);
    m_replaceAllBtn->hide();
    layout->addWidget(m_replaceAllBtn);

    layout->addStretch(1);

    connect(m_findEdit, &QLineEdit::textChanged, this, [this](const QString&) { refreshMatches(); });
    connect(m_caseBtn, &QToolButton::toggled, this, [this](bool) { refreshMatches(); });
    connect(m_wordBtn, &QToolButton::toggled, this, [this](bool) { refreshMatches(); });
    connect(m_regexBtn, &QToolButton::toggled, this, [this](bool) { refreshMatches(); });
    connect(m_nextBtn, &QToolButton::clicked, this, &FindReplaceBar::findNext);
    connect(m_prevBtn, &QToolButton::clicked, this, &FindReplaceBar::findPrevious);
    connect(m_replaceToggle, &QPushButton::clicked, this, [this]() {
        const bool show = m_replaceEdit->isHidden();
        m_replaceEdit->setVisible(show);
        m_replaceOneBtn->setVisible(show);
        m_replaceAllBtn->setVisible(show);
        if (show) m_replaceEdit->setFocus();
    });
    connect(m_replaceOneBtn, &QPushButton::clicked, this, &FindReplaceBar::replaceOne);
    connect(m_replaceAllBtn, &QPushButton::clicked, this, &FindReplaceBar::replaceAll);
    connect(closeBtn, &QToolButton::clicked, this, &QWidget::hide);
}

void FindReplaceBar::showEvent(QShowEvent* e)
{
    QWidget::showEvent(e);
    refreshMatches();
}

void FindReplaceBar::openFind()
{
    m_replaceEdit->hide();
    m_replaceOneBtn->hide();
    m_replaceAllBtn->hide();
    show();
    m_findEdit->setFocus();
    m_findEdit->selectAll();
    refreshMatches();
}

void FindReplaceBar::openReplace()
{
    m_replaceEdit->show();
    m_replaceOneBtn->show();
    m_replaceAllBtn->show();
    show();
    m_findEdit->setFocus();
    m_findEdit->selectAll();
    refreshMatches();
}

void FindReplaceBar::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Escape) {
        hide();
        m_editor->clearFindMatches();
        m_editor->setFocus();
        return;
    }
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        if (e->modifiers() & Qt::ShiftModifier) findPrevious();
        else findNext();
        return;
    }
    QWidget::keyPressEvent(e);
}

void FindReplaceBar::collectMatches()
{
    m_matches.clear();
    m_current = -1;
    const QString pattern = m_findEdit->text();
    if (pattern.isEmpty()) return;

    QTextDocument* doc = m_editor->document();
    const bool useRegex = m_regexBtn->isChecked();
    const bool caseSensitive = m_caseBtn->isChecked();
    const bool wholeWord = m_wordBtn->isChecked();

    if (useRegex) {
        QRegularExpression re(pattern,
                              caseSensitive ? QRegularExpression::NoPatternOption
                                            : QRegularExpression::CaseInsensitiveOption);
        if (!re.isValid()) return;
        QTextCursor c(doc);
        c.movePosition(QTextCursor::Start);
        int guard = 0;
        while (!c.atEnd() && guard++ < 10000) {
            c = doc->find(re, c);
            if (c.isNull()) break;
            m_matches.append(c);
        }
    } else {
        QString needle = pattern;
        if (wholeWord) needle = QStringLiteral("\\b%1\\b").arg(QRegularExpression::escape(needle));
        QRegularExpression re(needle,
                              caseSensitive ? QRegularExpression::NoPatternOption
                                            : QRegularExpression::CaseInsensitiveOption);
        QTextCursor c(doc);
        c.movePosition(QTextCursor::Start);
        int guard = 0;
        while (!c.atEnd() && guard++ < 10000) {
            c = doc->find(re, c);
            if (c.isNull()) break;
            m_matches.append(c);
        }
    }
}

void FindReplaceBar::refreshMatches()
{
    collectMatches();
    updateCountLabel();
    if (!m_matches.isEmpty()) jumpTo(0);
    else m_editor->setFindMatches(m_matches, -1);
}

void FindReplaceBar::updateCountLabel()
{
    auto* label = findChild<QLabel*>(QStringLiteral("find_count"));
    if (label)
        label->setText(m_matches.isEmpty() ? QStringLiteral("No results")
                                           : QStringLiteral("%1 of %2").arg(m_current + 1).arg(m_matches.size()));
}

void FindReplaceBar::jumpTo(int index)
{
    if (index < 0 || index >= m_matches.size()) return;
    m_current = index;
    QTextCursor c = m_matches.at(index);
    m_editor->setTextCursor(c);
    m_editor->setFindMatches(m_matches, index);
    m_editor->ensureCursorVisible();
    updateCountLabel();
}

void FindReplaceBar::findNext()
{
    if (m_matches.isEmpty()) return refreshMatches();
    jumpTo((m_current + 1) % m_matches.size());
}

void FindReplaceBar::findPrevious()
{
    if (m_matches.isEmpty()) return refreshMatches();
    jumpTo((m_current - 1 + m_matches.size()) % m_matches.size());
}

void FindReplaceBar::replaceOne()
{
    if (m_current < 0 || m_current >= m_matches.size()) return;
    QTextCursor c = m_matches.at(m_current);
    c.insertText(m_replaceEdit->text());
    refreshMatches();
}

void FindReplaceBar::replaceAll()
{
    if (m_matches.isEmpty()) return;
    QTextCursor c(m_editor->document());
    c.beginEditBlock();
    // Replace from last to first so earlier positions stay valid.
    for (int i = m_matches.size() - 1; i >= 0; --i) {
        QTextCursor mc = m_matches.at(i);
        mc.insertText(m_replaceEdit->text());
    }
    c.endEditBlock();
    refreshMatches();
}

}  // namespace cf
