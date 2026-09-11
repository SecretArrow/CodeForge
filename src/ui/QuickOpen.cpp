#include "ui/QuickOpen.h"

#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QVBoxLayout>

#include "core/CommandRegistry.h"
#include "settings/KeybindManager.h"
#include "core/FuzzyMatch.h"
#include "core/TextDocument.h"
#include "editor/EditorArea.h"
#include "project/Workspace.h"
#include "syntax/SymbolScanner.h"
#include "ui/MainWindow.h"

namespace cf {

QuickOpen::QuickOpen(QWidget* parent)
    : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint)
{
    setObjectName(QStringLiteral("codeforge_quickopen"));
    setMinimumSize(560, 60);
    setMaximumHeight(480);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    m_input = new QLineEdit(this);
    m_input->setPlaceholderText(tr("Type a file name, '>' for commands, ':' for line, '@' for symbols"));
    m_input->setClearButtonEnabled(true);
    layout->addWidget(m_input);

    m_list = new QListWidget(this);
    m_list->setUniformItemSizes(true);
    layout->addWidget(m_list, 1);

    connect(m_input, &QLineEdit::textChanged, this, &QuickOpen::refreshList);
    connect(m_input, &QLineEdit::returnPressed, this, [this]() {
        if (m_list->count() > 0) acceptItem(m_list->currentItem() ? m_list->currentItem() : m_list->item(0));
        hide();
    });
    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        acceptItem(item);
        hide();
    });

    installEventFilter(this);
    m_input->installEventFilter(this);
}

void QuickOpen::positionOverOwner()
{
    auto* mw = qobject_cast<MainWindow*>(parentWidget());
    if (!mw) return;
    const QSize hint = sizeHint();
    resize(qMin(mw->width() * 2 / 3, 720), qMin(mw->height() * 3 / 4, 460));
    move(mw->x() + (mw->width() - width()) / 2, mw->y() + mw->menuHeight() + 8);
    Q_UNUSED(hint);
}

void QuickOpen::showForMode(Mode mode)
{
    m_mode = mode;
    m_input->clear();
    switch (mode) {
    case Mode::Commands: m_input->setText(QStringLiteral(">")); break;
    case Mode::GotoLine: m_input->setText(QStringLiteral(":")); break;
    case Mode::Symbols:  m_input->setText(QStringLiteral("@")); break;
    default: break;
    }
    positionOverOwner();
    show();
    raise();
    m_input->setFocus();
    refreshList();
}

void QuickOpen::openFiles()    { showForMode(Mode::Files); }
void QuickOpen::openCommands() { showForMode(Mode::Commands); }
void QuickOpen::openGotoLine() { showForMode(Mode::GotoLine); }
void QuickOpen::openSymbols()  { showForMode(Mode::Symbols); }

bool QuickOpen::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (watched == m_input) {
            if (key->key() == Qt::Key_Escape) { hide(); return true; }
            if (key->key() == Qt::Key_Down) {
                if (m_list->count() > 0) m_list->setCurrentRow(qMax(0, m_list->currentRow() + 1));
                return true;
            }
            if (key->key() == Qt::Key_Up) {
                m_list->setCurrentRow(qMax(0, m_list->currentRow() - 1));
                return true;
            }
            // Mode switching by editing the prefix live.
            const QString text = m_input->text();
            if (key->key() == Qt::Key_Backspace && text == QStringLiteral(">") ) { hide(); return true; }
        }
    }
    if (event->type() == QEvent::Close) hide();
    return QWidget::eventFilter(watched, event);
}

void QuickOpen::refreshList()
{
    auto* mw = qobject_cast<MainWindow*>(parentWidget());
    m_list->clear();

    const QString raw = m_input->text();
    const QString query = raw.startsWith(QLatin1Char('>')) || raw.startsWith(QLatin1Char(':')) || raw.startsWith(QLatin1Char('@'))
                              ? raw.mid(1).trimmed() : raw;

    Mode mode = m_mode;
    if (raw.startsWith(QLatin1Char('>'))) mode = Mode::Commands;
    else if (raw.startsWith(QLatin1Char(':'))) mode = Mode::GotoLine;
    else if (raw.startsWith(QLatin1Char('@'))) mode = Mode::Symbols;

    if (mode == Mode::Files) {
        if (!mw || !mw->workspace().isOpen()) return;
        struct Entry { int score; QString path; };
        QVector<Entry> scored;
        for (const QString& rel : mw->workspace().indexedFiles()) {
            FuzzyResult r = FuzzyMatch::scorePath(query, rel);
            if (r.score >= 0) scored.append({r.score, rel});
        }
        std::stable_sort(scored.begin(), scored.end(), [](const Entry& a, const Entry& b) { return a.score > b.score; });
        int shown = 0;
        for (const Entry& e : scored) {
            if (shown++ >= 40) break;
            QListWidgetItem* item = new QListWidgetItem(mw->workspace().absolutePath(e.path), m_list);
            item->setData(Qt::UserRole, mw->workspace().absolutePath(e.path));
        }
    } else if (mode == Mode::Commands) {
        if (!mw) return;
        struct Entry { int score; const Command* cmd; };
        QVector<Entry> scored;
        for (const Command* cmd : mw->commands().commands()) {
            const QString needle = query.isEmpty() ? cmd->title : QStringLiteral("%1 %2 %3").arg(cmd->category, cmd->title, cmd->id);
            FuzzyResult r = FuzzyMatch::score(query, needle);
            if (r.score >= 0) scored.append({r.score, cmd});
        }
        std::stable_sort(scored.begin(), scored.end(), [](const Entry& a, const Entry& b) { return a.score > b.score; });
        int shown = 0;
        for (const Entry& e : scored) {
            if (shown++ >= 40) break;
            const QString label = QStringLiteral("%1: %2").arg(e.cmd->category, e.cmd->title);
            QListWidgetItem* item = new QListWidgetItem(label, m_list);
            const QKeySequence seq = mw->keybinds().effective(e.cmd->id);
            if (!seq.isEmpty()) item->setText(label + QStringLiteral("    (%1)").arg(seq.toString()));
            item->setData(Qt::UserRole, e.cmd->id);
        }
    } else if (mode == Mode::GotoLine) {
        bool ok = false;
        const int line = query.toInt(&ok);
        if (ok && line > 0) {
            QListWidgetItem* item = new QListWidgetItem(tr("Go to line %1").arg(line), m_list);
            item->setData(Qt::UserRole, line);
        }
    } else if (mode == Mode::Symbols) {
        TextDocument* doc = mw ? mw->activeDocument() : nullptr;
        if (!doc) return;
        const QString lang = doc->languageId();
        const QList<SymbolInfo> symbols = SymbolScanner::scan(lang, doc->document()->toPlainText());
        for (const SymbolInfo& sym : symbols) {
            FuzzyResult r = FuzzyMatch::score(query, sym.name);
            if (!query.isEmpty() && r.score < 0) continue;
            QListWidgetItem* item = new QListWidgetItem(QStringLiteral("%1  %2  (line %3)")
                                                            .arg(sym.kind, sym.name).arg(sym.line + 1), m_list);
            item->setData(Qt::UserRole, sym.line);
        }
    }

    if (m_list->count() > 0) m_list->setCurrentRow(0);
}

void QuickOpen::acceptItem(QListWidgetItem* item)
{
    if (!item) return;
    auto* mw = qobject_cast<MainWindow*>(parentWidget());
    if (!mw) return;

    const QVariant data = item->data(Qt::UserRole);
    switch (m_mode) {
    case Mode::Files:
        emit openFileRequested(data.toString(), true);
        break;
    case Mode::Commands:
        emit executeCommand(data.toString());
        break;
    case Mode::GotoLine:
        emit gotoLineRequested(data.toInt() - 1);
        break;
    case Mode::Symbols:
        emit gotoSymbolRequested(data.toInt());
        break;
    }
    hide();
}

}  // namespace cf
