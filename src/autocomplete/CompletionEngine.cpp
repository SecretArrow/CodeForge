#include "autocomplete/CompletionEngine.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QKeyEvent>
#include <QListWidget>
#include <QScreen>
#include <QTimer>
#include <QTextBlock>
#include <QTextCursor>
#include <QRegularExpression>
#include <QFontMetrics>
#include <algorithm>

#include "settings/SettingsManager.h"

namespace cf {

// ---------------------------------------------------------------------------
// CompletionPopup: focusless tooltip-style list window. The editor keeps
// keyboard focus; we only forward navigation keys from the interceptor.
// ---------------------------------------------------------------------------
struct CompletionPopup : QListWidget {
public:
    explicit CompletionPopup(QWidget* parent)
        : QListWidget(parent)
    {
        setWindowFlags(Qt::ToolTip | Qt::NoDropShadowWindowHint | Qt::FramelessWindowHint);
        setUniformItemSizes(true);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setFocusPolicy(Qt::NoFocus);
        setEditTriggers(QAbstractItemView::NoEditTriggers);
    }

    void adjustPopupSize(int rows)
    {
        const QFontMetrics fm(font());
        int w = 0;
        for (int i = 0; i < count(); ++i)
            w = qMax(w, fm.horizontalAdvance(item(i)->text()));
        w = qMin(qMax(w + 28, 220), 520);
        const int rowH = sizeHintForRow(0);
        const int h = rowH > 0 ? qMin(count(), qMax(3, rows)) * rowH + 10 : 120;
        resize(w, h);
    }
};

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------
QString CompletionEngine::wordBeforeCursor(const QTextCursor& cursor)
{
    static const QRegularExpression wordChars(QRegularExpression::anchoredPattern(
        QStringLiteral("[A-Za-z0-9_]+")));
    const QString left = cursor.block().text().left(cursor.positionInBlock());
    // longest suffix made of word characters
    int i = left.size();
    while (i > 0 && wordChars.match(left.mid(i - 1, 1)).hasMatch())
        --i;
    return left.mid(i);
}

namespace {

int fuzzyScore(const QString& word, const QString& prefix)
{
    if (prefix.isEmpty())
        return 10;
    if (!word.startsWith(prefix, Qt::CaseInsensitive))
        return -1;
    return word.startsWith(prefix) ? 0 : 1;
}

}  // namespace

QVector<CompletionItem> CompletionEngine::filterRank(const QVector<CompletionItem>& items,
                                                     const QString& prefix)
{
    QVector<QPair<int, const CompletionItem*>> scored;
    scored.reserve(items.size());
    for (const CompletionItem& it : items) {
        const int s = fuzzyScore(it.label, prefix);
        if (s >= 0)
            scored.append({ s * 1000 + it.kind * 10 + (it.isSnippet ? 5 : 0), &it });
    }
    std::stable_sort(scored.begin(), scored.end(),
                     [](const auto& a, const auto& b) { return a.first < b.first; });
    QVector<CompletionItem> out;
    out.reserve(qMin(50, scored.size()));
    for (int i = 0; i < scored.size() && out.size() < 50; ++i)
        out.append(*scored[i].second);
    return out;
}

// ---------------------------------------------------------------------------
// engine
// ---------------------------------------------------------------------------
CompletionEngine::CompletionEngine(CodeEditor* editor, QObject* parent)
    : QObject(parent)
    , m_editor(editor)
{
    m_popup = new CompletionPopup(nullptr);
    connect(m_popup, &QListWidget::itemClicked, this, [this](QListWidgetItem*) { acceptCurrent(); });

    m_debounceTimer = new QTimer(this);
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(120);
    connect(m_debounceTimer, &QTimer::timeout, this, &CompletionEngine::onDebounceTimeout);

    m_wordTimer = new QTimer(this);
    m_wordTimer->setSingleShot(true);
    m_wordTimer->setInterval(500);
    connect(m_wordTimer, &QTimer::timeout, this, &CompletionEngine::onWordScanTimeout);

    if (m_editor) {
        m_editor->addKeyInterceptor(this);
        connect(m_editor, &QPlainTextEdit::textChanged, this, [this]() {
            if (m_popup->isVisible() && !m_applying)
                closePopup();
            scheduleWordScan();
        });
        connect(m_editor, &QPlainTextEdit::cursorPositionChanged, this, [this]() {
            if (m_popup->isVisible() && !m_applying) {
                // Keep open only when the caret stayed within the word being completed.
                const QString now = wordBeforeCursor(m_editor->textCursor());
                if (!now.startsWith(m_prefix, Qt::CaseInsensitive))
                    closePopup();
            }
        });
        scheduleWordScan();
    }
}

CompletionEngine::~CompletionEngine()
{
    if (m_editor)
        m_editor->removeKeyInterceptor(this);
    if (m_popup) {
        m_popup->hide();
        m_popup->deleteLater();
    }
}

void CompletionEngine::setKeywords(const QStringList& words)
{
    m_keywordItems.clear();
    m_keywordItems.reserve(words.size());
    for (const QString& w : words) {
        if (w.isEmpty())
            continue;
        CompletionItem it;
        it.label = w;
        it.kind = 1;
        it.detail = QStringLiteral("keyword");
        m_keywordItems.append(it);
    }
}

void CompletionEngine::setSnippets(const QVector<CompletionItem>& snippets)
{
    m_snippetItems = snippets;
}

void CompletionEngine::setExternalProvider(
    std::function<void(const QString&, int, int, std::function<void(QVector<CompletionItem>)>)> provider)
{
    m_provider = std::move(provider);
}

bool CompletionEngine::isPopupVisible() const
{
    return m_popup && m_popup->isVisible();
}

void CompletionEngine::closePopup()
{
    ++m_generation;
    m_popup->hide();
    m_prefix.clear();
}

void CompletionEngine::triggerNow()
{
    if (!m_editor)
        return;
    const QString prefix = wordBeforeCursor(m_editor->textCursor());
    openPopup(prefix);
}

bool CompletionEngine::editorKeyPress(CodeEditor* editor, QKeyEvent* e)
{
    if (editor != m_editor)
        return false;

    if (m_popup->isVisible()) {
        switch (e->key()) {
        case Qt::Key_Escape:
            closePopup();
            return true;
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Tab:
            acceptCurrent();
            return true;
        case Qt::Key_Up:
            cycle(-1);
            return true;
        case Qt::Key_Down:
            cycle(1);
            return true;
        case Qt::Key_PageUp:
            cyclePage(-1);
            return true;
        case Qt::Key_PageDown:
            cyclePage(1);
            return true;
        case Qt::Key_Left:
        case Qt::Key_Right:
        case Qt::Key_Home:
        case Qt::Key_End:
            closePopup();
            return false;
        default:
            break;
        }
        // printable input -> let the editor insert it, then re-filter (queued)
        const QString text = e->text();
        if (!text.isEmpty() && text.at(0).isPrint() && !text.at(0).isSpace()) {
            QMetaObject::invokeMethod(this, &CompletionEngine::refreshFilter, Qt::QueuedConnection);
            return false;
        }
        if (e->key() == Qt::Key_Backspace) {
            QMetaObject::invokeMethod(this, &CompletionEngine::refreshFilter, Qt::QueuedConnection);
            return false;
        }
        closePopup();
        return false;
    }

    // popup closed: explicit trigger
    if (e->modifiers() & Qt::ControlModifier && e->key() == Qt::Key_Space) {
        triggerNow();
        return true;
    }

    // Tab expands a snippet trigger when no popup is open (falls through
    // otherwise so the editor keeps its indent behavior).
    if (e->key() == Qt::Key_Tab && !(e->modifiers() & (Qt::ControlModifier | Qt::AltModifier))) {
        if (expandSnippetAtCursor())
            return true;
    }

    // auto trigger on word characters
    const QString text = e->text();
    if (!text.isEmpty() && text.at(0).isLetterOrNumber()) {
        m_debounceTimer->start();
    }
    return false;
}

void CompletionEngine::cycle(int delta)
{
    const int count = m_popup->count();
    if (count == 0)
        return;
    int row = m_popup->currentRow() + delta;
    if (row < 0) row = count - 1;
    if (row >= count) row = 0;
    m_popup->setCurrentRow(row);
}

void CompletionEngine::cyclePage(int delta)
{
    const int count = m_popup->count();
    if (count == 0)
        return;
    int row = m_popup->currentRow() + delta * 8;
    row = qBound(0, row, count - 1);
    m_popup->setCurrentRow(row);
}

void CompletionEngine::onDebounceTimeout()
{
    if (!m_editor || m_editor->hasExtraCursors())
        return;
    const QString prefix = wordBeforeCursor(m_editor->textCursor());
    if (prefix.size() < 3)
        return;
    openPopup(prefix);
}

void CompletionEngine::openPopup(const QString& prefix)
{
    if (!m_editor)
        return;
    ++m_generation;
    m_prefix = prefix;
    m_filtered = filterRank(collectSources(prefix), prefix);
    if (m_filtered.isEmpty()) {
        closePopup();
        return;
    }
    m_popup->clear();
    for (const CompletionItem& it : m_filtered) {
        QString label = it.label;
        if (!it.detail.isEmpty())
            label += QStringLiteral("   \u2014 ") + it.detail;
        m_popup->addItem(label);
    }
    m_popup->setCurrentRow(0);

    // position under the caret, flipped above when there is no room below
    const QRect cr = m_editor->cursorRect();
    QPoint g = m_editor->viewport()->mapToGlobal(cr.bottomLeft());
    m_popup->adjustPopupSize(12);
    if (QScreen* screen = m_editor->screen()) {
        const QRect av = screen->availableGeometry();
        if (g.y() + m_popup->height() > av.bottom()) {
            const QPoint top = m_editor->viewport()->mapToGlobal(cr.topLeft());
            g.setY(qMax(av.top(), top.y() - m_popup->height()));
        }
        g.setX(qMin(g.x(), av.right() - m_popup->width()));
        g.setY(qMin(qMax(g.y(), av.top()), qMax(av.top(), av.bottom() - m_popup->height())));
    }
    m_popup->move(g);
    m_popup->show();
    if (m_provider && m_externalPrefix != prefix) {
        m_external.clear();
        m_externalPrefix = prefix;
        requestExternal(prefix);
    }
    emit popupOpened();
}

QVector<CompletionItem> CompletionEngine::collectSources(const QString& prefix) const
{
    QVector<CompletionItem> items;
    items += m_keywordItems;
    items += m_snippetItems;
    items += m_external;
    for (const QString& w : m_docWords) {
        CompletionItem it;
        it.label = w;
        it.kind = 3;
        items.append(it);
    }
    return items;
}

void CompletionEngine::refreshFilter()
{
    if (!m_popup->isVisible() || !m_editor)
        return;
    const QString prefix = wordBeforeCursor(m_editor->textCursor());
    if (prefix.isEmpty() || !prefix.startsWith(m_prefix, Qt::CaseInsensitive)) {
        closePopup();   // caret left the word being completed
        return;
    }
    m_prefix = prefix;
    const int gen = m_generation;
    openPopup(prefix);
    m_generation = gen;   // refreshFilter keeps the popup "session" alive
}

void CompletionEngine::requestExternal(const QString& prefix)
{
    if (!m_provider || !m_editor)
        return;
    const int gen = m_generation;   // results only valid while this session is open
    const int line = m_editor->textCursor().blockNumber();
    const int col = m_editor->textCursor().positionInBlock();
    auto cb = [this, gen, prefix](QVector<CompletionItem> results) {
        QMetaObject::invokeMethod(this, [this, gen, prefix, results]() {
            onExternalResults(gen, prefix, results);
        }, Qt::QueuedConnection);
    };
    m_provider(prefix, line, col, cb);
}

void CompletionEngine::onExternalResults(int generation, const QString& prefix,
                                          QVector<CompletionItem> results)
{
    if (generation != m_generation || !m_popup->isVisible())
        return;
    if (prefix != m_prefix)
        return;
    m_external = filterRank(results, prefix);
    // re-show with merged results
    openPopup(m_prefix);
}

void CompletionEngine::acceptCurrent()
{
    const int row = m_popup->currentRow();
    if (row < 0 || row >= m_filtered.size())
        return;
    acceptItem(m_filtered.at(row));
}

void CompletionEngine::acceptItem(const CompletionItem& item)
{
    if (!m_editor)
        return;
    ++m_generation;
    m_popup->hide();

    QTextCursor c = m_editor->textCursor();
    // remove the typed prefix; the item insert replaces it
    if (!m_prefix.isEmpty())
        c.setPosition(c.position() - m_prefix.size(), QTextCursor::KeepAnchor);

    m_applying = true;
    if (item.isSnippet && !item.snippetBody.isEmpty()) {
        insertSnippetBody(c, item.snippetBody);
    } else {
        c.insertText(item.insertText.isEmpty() ? item.label : item.insertText);
        m_editor->setTextCursor(c);
    }
    m_applying = false;
    emit itemActivated(item);
}

bool CompletionEngine::expandSnippetAtCursor()
{
    if (!m_editor || m_editor->hasExtraCursors())
        return false;
    const QString word = wordBeforeCursor(m_editor->textCursor());
    if (word.isEmpty())
        return false;
    for (const CompletionItem& it : m_snippetItems) {
        if (it.label == word && it.isSnippet && !it.snippetBody.isEmpty()) {
            acceptItem(it);
            return true;
        }
    }
    return false;
}

void CompletionEngine::insertSnippetBody(QTextCursor c, const QString& body)
{
    // $0 marks the final caret position; everything else is literal text.
    QString text = body;
    int marker = text.indexOf(QLatin1String("$0"));
    int caretOffset = -1;
    if (marker >= 0) {
        caretOffset = marker;
        text.remove(marker, 2);
    }
    c.insertText(text);
    if (caretOffset >= 0) {
        c.setPosition(c.position() - text.size() + caretOffset);
        m_editor->setTextCursor(c);
    } else {
        m_editor->setTextCursor(c);
    }
}

void CompletionEngine::scheduleWordScan()
{
    m_wordTimer->start();
}

void CompletionEngine::onWordScanTimeout()
{
    scanDocumentWords();
    if (m_popup->isVisible())
        m_filtered = filterRank(collectSources(m_prefix), m_prefix);
}

void CompletionEngine::scanDocumentWords()
{
    if (!m_editor)
        return;
    QTextDocument* doc = m_editor->document();
    if (!doc)
        return;
    const int totalChars = doc->characterCount();
    if (totalChars > 2 * 1000 * 1000) {
        m_docWords.clear();
        return;
    }
    static const QRegularExpression re(QStringLiteral("[A-Za-z_][A-Za-z0-9_]{2,}"));
    QSet<QString> words;
    words.reserve(4096);
    QTextBlock b = doc->begin();
    while (b.isValid() && words.size() < 20000) {
        const QString t = b.text();
        auto it = re.globalMatch(t);
        while (it.hasNext() && words.size() < 20000) {
            const auto m = it.next();
            words.insert(m.captured(0));
        }
        b = b.next();
    }
    m_docWords = words;
}

}  // namespace cf
