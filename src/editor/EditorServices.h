#pragma once
// EditorServices: global hooks run for every CodeEditor created by any
// EditorGroup. Feature modules (completion, modal editing, snippets...)
// register an init hook at startup instead of EditorGroup depending on each
// feature directly (keeps the dependency direction editor -> features off).
#include <functional>

#include <QString>

class CodeEditor;   // fwd (global)

namespace cf {

class EditorServices {
public:
    using Hook = std::function<void(CodeEditor*)>;

    static EditorServices& instance();

    // Hooks run in registration order when an editor is created.
    void addInitHook(Hook hook);

    // Called by EditorGroup::createEditor().
    void notifyEditorCreated(CodeEditor* editor);

private:
    EditorServices() = default;
    QList<Hook> m_hooks;
};

}  // namespace cf
