#include "core/CommandRegistry.h"

#include <algorithm>

namespace cf {

CommandRegistry::CommandRegistry(QObject* parent) : QObject(parent) {}

void CommandRegistry::registerCommand(const QString& id, const QString& category, const QString& title,
                                      const QKeySequence& defaultShortcut, std::function<void()> fn)
{
    if (m_commands.contains(id)) {
        m_commands[id].run = std::move(fn);
        return;
    }
    Command cmd;
    cmd.id = id;
    cmd.category = category;
    cmd.title = title;
    cmd.defaultShortcut = defaultShortcut;
    cmd.run = std::move(fn);
    m_commands.insert(id, cmd);
}

bool CommandRegistry::execute(const QString& id) const
{
    const auto it = m_commands.constFind(id);
    if (it == m_commands.constEnd() || !it->run) return false;
    it->run();
    return true;
}

const Command* CommandRegistry::command(const QString& id) const
{
    const auto it = m_commands.constFind(id);
    return it == m_commands.constEnd() ? nullptr : &it.value();
}

QList<const Command*> CommandRegistry::commands() const
{
    QList<const Command*> out;
    out.reserve(m_commands.size());
    for (const Command& c : m_commands) out.append(&c);
    std::sort(out.begin(), out.end(), [](const Command* a, const Command* b) {
        if (a->category != b->category) return a->category < b->category;
        return a->title < b->title;
    });
    return out;
}

}  // namespace cf
