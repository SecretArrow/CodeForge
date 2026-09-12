#pragma once
// VimModal / EmacsModal: modal editing for CodeEditor implemented as
// EditorKeyInterceptors. State is per editor; the keymaps cover the practical
// editing core (see the .cpp for the exact tables).
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QStringList>

#include "editor/CodeEditor.h"

class QKeyEvent;

namespace cf {

class VimModal : public QObject, public EditorKeyInterceptor {
    Q_OBJECT
public:
    explicit VimModal(QObject* parent = nullptr);

    void attachTo(CodeEditor* editor);     // starts intercepting + sets state
    void detachFrom(CodeEditor* editor);
    bool attachedTo(CodeEditor* editor) const { return m_states.contains(editor); }
    void detachAll();

    enum class Mode { Normal, Insert, Visual };
    Mode modeFor(CodeEditor* editor) const;
    QString modeLabelFor(CodeEditor* editor) const;
    QString pendingLabelFor(CodeEditor* editor) const;

    bool editorKeyPress(CodeEditor* editor, QKeyEvent* e) override;

signals:
    void modeChanged(CodeEditor* editor, const QString& label);
    void commandRequested(CodeEditor* editor, const QString& cmd);   // ":w", ":q", ...
    void searchRequested(CodeEditor* editor, const QString& needle);

private:
    struct State;   // defined in the .cpp
    void setMode(CodeEditor* editor, State& st, Mode m);
    void cleanupDestroyed();

    QHash<CodeEditor*, State*> m_states;
};

class EmacsModal : public QObject, public EditorKeyInterceptor {
    Q_OBJECT
public:
    explicit EmacsModal(QObject* parent = nullptr);

    void attachTo(CodeEditor* editor);
    void detachFrom(CodeEditor* editor);
    void detachAll();

    bool editorKeyPress(CodeEditor* editor, QKeyEvent* e) override;

signals:
    void commandRequested(CodeEditor* editor, const QString& cmd);   // "save", "find", ...
    void statusMessage(CodeEditor* editor, const QString& msg);

private:
    QStringList m_killRing;                       // C-k / M-w push, C-y pops
    QSet<CodeEditor*> m_attached;
    QSet<CodeEditor*> m_markSet;                  // C-Space transient mark mode
};

}  // namespace cf
