#include "editormodes/VimEmacsModes.h"

#include <QInputDialog>
#include <QKeyEvent>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

namespace cf {

// ===========================================================================
// VimModal
// ===========================================================================

struct VimModal::State {
    Mode mode = Mode::Normal;
    int count = 0;              // pending count
    QChar pendingOp;            // 'd', 'c', 'y' awaiting a motion
    QChar pendingG = 0;         // 'g' awaiting 'g'
    QString registerText;       // yank/delete register
    bool registerIsLine = false;
    QString lastSearch;
    int visualAnchor = -1;      // char position when 'v' was pressed
};

VimModal::VimModal(QObject* parent)
    : QObject(parent)
{
}

void VimModal::attachTo(CodeEditor* editor)
{
    if (!editor || m_states.contains(editor))
        return;
    auto* st = new State();
    m_states.insert(editor, st);
    editor->addKeyInterceptor(this);
    setMode(editor, *st, Mode::Normal);
    connect(editor, &QObject::destroyed, this, [this, editor]() {
        if (State* s = m_states.take(editor))
            delete s;
    });
}

void VimModal::detachFrom(CodeEditor* editor)
{
    if (!editor)
        return;
    if (State* st = m_states.take(editor)) {
        delete st;
        editor->removeKeyInterceptor(this);
        emit modeChanged(editor, QString());
    }
}

void VimModal::detachAll()
{
    const auto editors = m_states.keys();
    for (CodeEditor* e : editors) {
        if (e)
            detachFrom(e);
        if (State* s = m_states.take(e))
            delete s;
    }
}

VimModal::Mode VimModal::modeFor(CodeEditor* editor) const
{
    if (State* st = m_states.value(editor, nullptr))
        return st->mode;
    return Mode::Normal;
}

QString VimModal::modeLabelFor(CodeEditor* editor) const
{
    if (State* st = m_states.value(editor, nullptr)) {
        switch (st->mode) {
        case Mode::Normal: return QStringLiteral("NORMAL");
        case Mode::Insert: return QStringLiteral("INSERT");
        case Mode::Visual: return QStringLiteral("VISUAL");
        }
    }
    return QString();
}

QString VimModal::pendingLabelFor(CodeEditor* editor) const
{
    if (State* st = m_states.value(editor, nullptr)) {
        QString label;
        if (st->count > 0)
            label += QString::number(st->count);
        if (!st->pendingOp.isNull())
            label += st->pendingOp;
        if (st->pendingG != 0)
            label += st->pendingG;
        return label;
    }
    return QString();
}

void VimModal::setMode(CodeEditor* editor, State& st, Mode m)
{
    st.mode = m;
    st.pendingOp = QChar();
    st.pendingG = 0;
    st.count = 0;
    emit modeChanged(editor, modeLabelFor(editor));
}

// linewise selection of `count` blocks starting at the cursor's block
static QTextCursor lineSelection(CodeEditor* editor, int count)
{
    QTextCursor c = editor->textCursor();
    c.clearSelection();
    QTextBlock first = c.block();
    QTextBlock last = first;
    for (int i = 1; i < qMax(1, count); ++i) {
        if (!last.next().isValid())
            break;
        last = last.next();
    }
    c.setPosition(first.position());
    c.setPosition(last.position() + last.length() - 1, QTextCursor::KeepAnchor);
    return c;
}

// move by a vim motion. keepAnchor=true extends the selection (visual mode
// and operators); returns the resulting cursor.
static QTextCursor moveTo(CodeEditor* editor, const QString& motion, int count, bool keepAnchor)
{
    QTextCursor c = editor->textCursor();
    c.clearSelection();
    const int n = qMax(1, count);
    QTextCursor::MoveOperation op = QTextCursor::NoMove;
    int steps = 1;
    if (motion == QLatin1String("h")) { op = QTextCursor::Left; steps = n; }
    else if (motion == QLatin1String("l")) { op = QTextCursor::Right; steps = n; }
    else if (motion == QLatin1String("j")) { op = QTextCursor::Down; steps = n; }
    else if (motion == QLatin1String("k")) { op = QTextCursor::Up; steps = n; }
    else if (motion == QLatin1String("w")) { op = QTextCursor::NextWord; steps = n; }
    else if (motion == QLatin1String("b")) { op = QTextCursor::PreviousWord; steps = n; }
    else if (motion == QLatin1String("e")) { op = QTextCursor::EndOfWord; steps = n; }
    else if (motion == QLatin1String("{")) { op = QTextCursor::PreviousBlock; steps = n; }
    else if (motion == QLatin1String("}")) { op = QTextCursor::NextBlock; steps = n; }
    else if (motion == QLatin1String("$")) op = QTextCursor::EndOfLine;
    else if (motion == QLatin1String("0")) op = QTextCursor::StartOfLine;
    else if (motion == QLatin1String("^")) op = QTextCursor::StartOfBlock;
    else if (motion == QLatin1String("G")) {
        if (count > 0) {
            QTextBlock b = editor->document()->findBlockByNumber(count - 1);
            if (b.isValid())
                c.setPosition(b.position());
        } else {
            op = QTextCursor::End;
        }
    }
    if (op != QTextCursor::NoMove)
        c.movePosition(op, keepAnchor ? QTextCursor::KeepAnchor : QTextCursor::MoveAnchor, steps);
    editor->setTextCursor(c);
    return c;
}

bool VimModal::editorKeyPress(CodeEditor* editor, QKeyEvent* e)
{
    auto it = m_states.find(editor);
    if (it == m_states.end() || !it.value())
        return false;
    State& st = *it.value();
    const int key = e->key();
    const Qt::KeyboardModifiers mods = e->modifiers() & ~Qt::KeypadModifier;
    const QString text = e->text();
    const QChar ch = text.isEmpty() ? QChar() : text.at(0);
    const bool plainChar = (mods == Qt::NoModifier && !text.isEmpty() && ch.isPrint());

    // ---- Insert mode: everything passes through except Escape / C-[ ----
    if (st.mode == Mode::Insert) {
        if (key == Qt::Key_Escape
            || (mods & Qt::ControlModifier && key == Qt::Key_BracketLeft)) {
            QTextCursor c = editor->textCursor();
            if (c.positionInBlock() > 0)
                c.movePosition(QTextCursor::Left);
            editor->setTextCursor(c);
            setMode(editor, st, Mode::Normal);
            return true;
        }
        return false;
    }

    // ---- Ex commands (":") via a small modal input ----
    if (key == Qt::Key_Colon && mods == Qt::NoModifier) {
        const QString cmd = QInputDialog::getText(editor, QStringLiteral("Vim command"),
                                                  QStringLiteral(":"));
        if (!cmd.isEmpty())
            emit commandRequested(editor, cmd);
        return true;
    }

    // ---- count digits ----
    if (key >= Qt::Key_1 && key <= Qt::Key_9 && mods == Qt::NoModifier) {
        st.count = st.count * 10 + (key - Qt::Key_0);
        emit modeChanged(editor, modeLabelFor(editor));
        return true;
    }

    // ---- pending 'g' (gg = top of buffer) ----
    if (st.pendingG != 0) {
        st.pendingG = 0;
        if (key == Qt::Key_G) {
            QTextCursor c = editor->textCursor();
            c.movePosition(QTextCursor::Start);
            editor->setTextCursor(c);
            emit modeChanged(editor, modeLabelFor(editor));
            return true;
        }
        // fall through: treat as normal key
    }

    // ---- pending operator (d/c/y) + motion ----
    if (!st.pendingOp.isNull()) {
        const QChar op = st.pendingOp;
        const int cnt = qMax(1, st.count);
        st.pendingOp = QChar();
        st.count = 0;
        if (plainChar && ch == op) {   // dd / cc / yy  (linewise)
            QTextCursor sel = lineSelection(editor, cnt);
            st.registerText = sel.selectedText();
            st.registerIsLine = true;
            if (op == 'y') {
                sel.setPosition(sel.anchor());
                editor->setTextCursor(sel);
            } else {
                sel.removeSelectedText();
                if (op == 'c') {
                    QTextCursor c = editor->textCursor();
                    c.insertBlock();
                    c.movePosition(QTextCursor::Up);
                    editor->setTextCursor(c);
                    setMode(editor, st, Mode::Insert);
                }
            }
            emit modeChanged(editor, modeLabelFor(editor));
            return true;
        }

        QString motion;
        if (key == Qt::Key_H) motion = QStringLiteral("h");
        else if (key == Qt::Key_J) motion = QStringLiteral("j");
        else if (key == Qt::Key_K) motion = QStringLiteral("k");
        else if (key == Qt::Key_L) motion = QStringLiteral("l");
        else if (key == Qt::Key_W) motion = QStringLiteral("w");
        else if (key == Qt::Key_B) motion = QStringLiteral("b");
        else if (key == Qt::Key_E) motion = QStringLiteral("e");
        else if (key == Qt::Key_Dollar) motion = QStringLiteral("$");
        else if (key == Qt::Key_0) motion = QStringLiteral("0");
        else if (key == Qt::Key_AsciiCircum) motion = QStringLiteral("^");
        else if (key == Qt::Key_G) motion = QStringLiteral("G");
        else if (key == Qt::Key_BraceLeft) motion = QStringLiteral("{");
        else if (key == Qt::Key_BraceRight) motion = QStringLiteral("}");

        if (!motion.isEmpty()) {
            const int origin = editor->textCursor().position();
            QTextCursor dst = moveTo(editor, motion, cnt, false);
            QTextCursor sel = editor->textCursor();
            sel.setPosition(origin);
            sel.setPosition(dst.position(), QTextCursor::KeepAnchor);
            st.registerText = sel.selectedText();
            st.registerIsLine = false;
            if (op == 'y') {
                sel.setPosition(sel.anchor());
                editor->setTextCursor(sel);
            } else {
                sel.removeSelectedText();
                if (op == 'c')
                    setMode(editor, st, Mode::Insert);
            }
            emit modeChanged(editor, modeLabelFor(editor));
            return true;
        }
        // unknown motion: cancel operator
        emit modeChanged(editor, modeLabelFor(editor));
        return true;
    }

    // ---- operators ----
    if (plainChar && (ch == 'd' || ch == 'c' || ch == 'y')) {
        st.pendingOp = ch;
        emit modeChanged(editor, modeLabelFor(editor));
        return true;
    }

    // ---- visual mode ----
    if (st.mode == Mode::Visual) {
        QString motion;
        if (key == Qt::Key_H) motion = QStringLiteral("h");
        else if (key == Qt::Key_J) motion = QStringLiteral("j");
        else if (key == Qt::Key_K) motion = QStringLiteral("k");
        else if (key == Qt::Key_L) motion = QStringLiteral("l");
        else if (key == Qt::Key_W) motion = QStringLiteral("w");
        else if (key == Qt::Key_B) motion = QStringLiteral("b");
        else if (key == Qt::Key_E) motion = QStringLiteral("e");
        else if (key == Qt::Key_Dollar) motion = QStringLiteral("$");
        else if (key == Qt::Key_G) motion = QStringLiteral("G");
        if (!motion.isEmpty()) {
            moveTo(editor, motion, qMax(1, st.count), true);
            st.count = 0;
            return true;
        }
        if (plainChar && (ch == 'd' || ch == 'x')) {
            QTextCursor sel = editor->textCursor();
            if (sel.hasSelection()) {
                st.registerText = sel.selectedText();
                st.registerIsLine = false;
                sel.removeSelectedText();
            }
            setMode(editor, st, Mode::Normal);
            return true;
        }
        if (plainChar && ch == 'y') {
            QTextCursor sel = editor->textCursor();
            if (sel.hasSelection()) {
                st.registerText = sel.selectedText();
                st.registerIsLine = false;
            }
            setMode(editor, st, Mode::Normal);
            return true;
        }
        if (plainChar && (ch == 'c' || ch == 's')) {
            setMode(editor, st, Mode::Insert);
            return true;
        }
        if (key == Qt::Key_Escape) {
            setMode(editor, st, Mode::Normal);
            if (st.visualAnchor >= 0) {
                QTextCursor c = editor->textCursor();
                c.setPosition(st.visualAnchor);
                editor->setTextCursor(c);
                st.visualAnchor = -1;
            }
            return true;
        }
        return true;   // consume other keys in visual mode
    }

    // ---- plain motions (normal mode) ----
    if (plainChar) {
        QString motion;
        if (ch == 'h') motion = QStringLiteral("h");
        else if (ch == 'j') motion = QStringLiteral("j");
        else if (ch == 'k') motion = QStringLiteral("k");
        else if (ch == 'l') motion = QStringLiteral("l");
        else if (ch == 'w') motion = QStringLiteral("w");
        else if (ch == 'b') motion = QStringLiteral("b");
        else if (ch == 'e') motion = QStringLiteral("e");
        else if (ch == '0') motion = QStringLiteral("0");
        else if (ch == '$') motion = QStringLiteral("$");
        else if (ch == '^') motion = QStringLiteral("^");
        else if (ch == '{') motion = QStringLiteral("{");
        else if (ch == '}') motion = QStringLiteral("}");
        else if (ch == 'G') motion = QStringLiteral("G");
        else if (ch == 'g') { st.pendingG = 'g'; return true; }

        if (!motion.isEmpty()) {
            moveTo(editor, motion, st.count, false);
            st.count = 0;
            return true;
        }

        switch (ch.unicode()) {
        case 'i':
            setMode(editor, st, Mode::Insert);
            return true;
        case 'a': {
            QTextCursor c = editor->textCursor();
            if (c.positionInBlock() < c.block().length() - 1)
                c.movePosition(QTextCursor::Right);
            editor->setTextCursor(c);
            setMode(editor, st, Mode::Insert);
            return true;
        }
        case 'A': {
            QTextCursor c = editor->textCursor();
            c.movePosition(QTextCursor::EndOfBlock);
            editor->setTextCursor(c);
            setMode(editor, st, Mode::Insert);
            return true;
        }
        case 'I': {
            QTextCursor c = editor->textCursor();
            c.movePosition(QTextCursor::StartOfBlock);
            while (!c.atBlockEnd() && c.block().text().at(c.positionInBlock()).isSpace())
                c.movePosition(QTextCursor::Right);
            editor->setTextCursor(c);
            setMode(editor, st, Mode::Insert);
            return true;
        }
        case 'o':
        case 'O': {
            QTextCursor c = editor->textCursor();
            if (ch == 'o')
                c.movePosition(QTextCursor::EndOfBlock);
            else
                c.movePosition(QTextCursor::StartOfBlock);
            c.insertBlock();
            if (ch == 'O')
                c.movePosition(QTextCursor::Up);
            editor->setTextCursor(c);
            setMode(editor, st, Mode::Insert);
            return true;
        }
        case 'x': {
            QTextCursor c = editor->textCursor();
            if (!c.hasSelection())
                c.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, qMax(1, st.count));
            st.registerText = c.selectedText();
            st.registerIsLine = false;
            c.removeSelectedText();
            editor->setTextCursor(c);
            st.count = 0;
            return true;
        }
        case 'X': {
            QTextCursor c = editor->textCursor();
            c.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, qMax(1, st.count));
            st.registerText = c.selectedText();
            st.registerIsLine = false;
            c.removeSelectedText();
            editor->setTextCursor(c);
            st.count = 0;
            return true;
        }
        case 'D': {
            QTextCursor c = editor->textCursor();
            c.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            st.registerText = c.selectedText();
            st.registerIsLine = false;
            c.removeSelectedText();
            editor->setTextCursor(c);
            return true;
        }
        case 'C': {
            QTextCursor c = editor->textCursor();
            c.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            st.registerText = c.selectedText();
            st.registerIsLine = false;
            c.removeSelectedText();
            editor->setTextCursor(c);
            setMode(editor, st, Mode::Insert);
            return true;
        }
        case 'S': {
            QTextCursor sel = lineSelection(editor, 1);
            sel.removeSelectedText();
            editor->setTextCursor(sel);
            setMode(editor, st, Mode::Insert);
            return true;
        }
        case 'p':
        case 'P': {
            QTextCursor c = editor->textCursor();
            if (st.registerIsLine) {
                if (ch == 'P')
                    c.movePosition(QTextCursor::StartOfBlock);
                else
                    c.movePosition(QTextCursor::EndOfBlock);
                c.insertBlock();
                c.insertText(st.registerText.trimmed());
            } else {
                if (ch == 'p' && c.positionInBlock() < c.block().length() - 1)
                    c.movePosition(QTextCursor::Right);
                c.insertText(st.registerText);
            }
            editor->setTextCursor(c);
            return true;
        }
        case 'u':
            editor->undo();
            return true;
        case 'v': {
            setMode(editor, st, Mode::Visual);
            st.visualAnchor = editor->textCursor().position();
            return true;
        }
        case 'n':
        case 'N': {
            if (!st.lastSearch.isEmpty()) {
                QTextDocument::FindFlags flags;
                if (ch == 'N')
                    flags |= QTextDocument::FindBackward;
                editor->find(st.lastSearch, flags);
            }
            return true;
        }
        case '/':
        case '?':
            emit searchRequested(editor, QString());
            return true;
        default:
            break;
        }
    }

    if (key == Qt::Key_Escape) {
        st.pendingOp = QChar();
        st.pendingG = 0;
        st.count = 0;
        editor->clearExtraCursors();
        emit modeChanged(editor, modeLabelFor(editor));
        return true;
    }

    return false;
}

// ===========================================================================
// EmacsModal
// ===========================================================================

EmacsModal::EmacsModal(QObject* parent)
    : QObject(parent)
{
}

void EmacsModal::attachTo(CodeEditor* editor)
{
    if (!editor || m_attached.contains(editor))
        return;
    m_attached.insert(editor);
    editor->addKeyInterceptor(this);
    connect(editor, &QObject::destroyed, this, [this, editor]() { m_attached.remove(editor); });
    emit statusMessage(editor, QStringLiteral("Emacs keybindings active (C-q Esc-style quit via settings)"));
}

void EmacsModal::detachFrom(CodeEditor* editor)
{
    if (!editor)
        return;
    if (m_attached.remove(editor))
        editor->removeKeyInterceptor(this);
    emit statusMessage(editor, QString());
}

void EmacsModal::detachAll()
{
    const auto editors = m_attached.values();
    for (CodeEditor* e : editors) {
        if (e)
            detachFrom(e);
        else
            m_attached.remove(e);
    }
}

bool EmacsModal::editorKeyPress(CodeEditor* editor, QKeyEvent* e)
{
    if (!m_attached.contains(editor))
        return false;
    const int key = e->key();
    const bool ctrl = e->modifiers() & Qt::ControlModifier;
    const bool alt = e->modifiers() & Qt::AltModifier;
    QTextCursor c = editor->textCursor();
    c.setVisualNavigation(true);

    if (ctrl && !alt) {
        switch (key) {
        case Qt::Key_F: c.movePosition(QTextCursor::Right); break;
        case Qt::Key_B: c.movePosition(QTextCursor::Left); break;
        case Qt::Key_N: c.movePosition(QTextCursor::Down); break;
        case Qt::Key_P: c.movePosition(QTextCursor::Up); break;
        case Qt::Key_A: c.movePosition(QTextCursor::StartOfLine); break;
        case Qt::Key_E: c.movePosition(QTextCursor::EndOfLine); break;
        case Qt::Key_V: c.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, 20); break;
        case Qt::Key_D: c.deleteChar(); break;
        case Qt::Key_K: {
            if (c.atBlockEnd()) {
                if (c.block().next().isValid())
                    c.setPosition(c.position() + 1, QTextCursor::KeepAnchor);
            } else {
                c.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            }
            const QString killed = c.selectedText();
            if (!killed.isEmpty())
                m_killRing.prepend(killed);
            if (m_killRing.size() > 60)
                m_killRing.removeLast();
            c.removeSelectedText();
            break;
        }
        case Qt::Key_Y: {
            if (!m_killRing.isEmpty())
                c.insertText(m_killRing.first());
            break;
        }
        case Qt::Key_Space:
            m_markSet.insert(editor);
            emit statusMessage(editor, QStringLiteral("Mark set"));
            break;
        case Qt::Key_G:
            m_markSet.remove(editor);
            emit statusMessage(editor, QStringLiteral("Quit"));
            break;
        case Qt::Key_S:
            emit commandRequested(editor, QStringLiteral("find"));
            break;
        case Qt::Key_Underscore:
            editor->undo();
            break;
        default:
            return false;   // standard clipboard shortcuts etc. still work
        }
        editor->setTextCursor(c);
        return true;
    }

    if (alt && !ctrl) {
        switch (key) {
        case Qt::Key_F: c.movePosition(QTextCursor::NextWord); break;
        case Qt::Key_B: c.movePosition(QTextCursor::PreviousWord); break;
        case Qt::Key_V: c.movePosition(QTextCursor::Up, QTextCursor::MoveAnchor, 20); break;
        case Qt::Key_Less: c.movePosition(QTextCursor::Start); break;
        case Qt::Key_Greater: c.movePosition(QTextCursor::End); break;
        case Qt::Key_W: {
            QTextCursor sel = c;
            if (!sel.hasSelection())
                sel.select(QTextCursor::LineUnderCursor);
            const QString grabbed = sel.selectedText();
            if (!grabbed.isEmpty())
                m_killRing.prepend(grabbed);
            if (m_killRing.size() > 60)
                m_killRing.removeLast();
            editor->copy();
            emit statusMessage(editor, QStringLiteral("Saved to kill ring"));
            return true;
        }
        case Qt::Key_D:
            c.movePosition(QTextCursor::NextWord, QTextCursor::KeepAnchor);
            c.removeSelectedText();
            break;
        default:
            return false;
        }
        editor->setTextCursor(c);
        return true;
    }

    // C-x chords: accept C-x followed by a plain letter via a pending flag.
    return false;
}

}  // namespace cf
