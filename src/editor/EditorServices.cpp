#include "editor/EditorServices.h"

#include "editor/CodeEditor.h"

namespace cf {

EditorServices& EditorServices::instance()
{
    static EditorServices s;
    return s;
}

void EditorServices::addInitHook(Hook hook)
{
    if (hook)
        m_hooks.append(std::move(hook));
}

void EditorServices::notifyEditorCreated(CodeEditor* editor)
{
    if (!editor)
        return;
    for (const Hook& hook : m_hooks)
        hook(editor);
}

}  // namespace cf
