#include "settings/KeybindManager.h"

#include "core/CommandRegistry.h"
#include "settings/SettingsManager.h"

namespace cf {

KeybindManager::KeybindManager(CommandRegistry* commands, SettingsManager* settings, QObject* parent)
    : QObject(parent), m_commands(commands), m_settings(settings)
{
}

void KeybindManager::load()
{
    m_overrides.clear();
    const QHash<QString, QVariant> raw = m_settings->get(QStringLiteral("keyboard.overrides")).toHash();
    for (auto it = raw.constBegin(); it != raw.constEnd(); ++it) {
        const QKeySequence seq(it.value().toString());
        if (!seq.isEmpty()) m_overrides.insert(it.key(), seq);
    }
}

QKeySequence KeybindManager::effective(const QString& commandId) const
{
    const auto it = m_overrides.constFind(commandId);
    if (it != m_overrides.constEnd()) return it.value();
    const Command* cmd = m_commands->command(commandId);
    return cmd ? cmd->defaultShortcut : QKeySequence();
}

QStringList KeybindManager::conflicts(const QKeySequence& seq, const QString& exceptId) const
{
    QStringList out;
    if (seq.isEmpty()) return out;
    for (const Command* cmd : m_commands->commands()) {
        if (cmd->id == exceptId) continue;
        if (effective(cmd->id) == seq) out.append(cmd->id);
    }
    return out;
}

bool KeybindManager::setBinding(const QString& commandId, const QKeySequence& seq, QString* conflictWith)
{
    const QStringList c = conflicts(seq, commandId);
    if (!c.isEmpty()) {
        if (conflictWith) *conflictWith = c.first();
        return false;
    }
    if (seq.isEmpty()) {
        m_overrides.remove(commandId);
        const Command* cmd = m_commands->command(commandId);
        if (cmd && !cmd->defaultShortcut.isEmpty())
            m_overrides.insert(commandId, cmd->defaultShortcut);
    } else {
        m_overrides.insert(commandId, seq);
    }
    persist();
    emit bindingsChanged();
    return true;
}

bool KeybindManager::isOverridden(const QString& commandId) const
{
    return m_overrides.contains(commandId);
}

void KeybindManager::persist()
{
    // Default (non-overridden) sequences are simply removed from the map.
    QHash<QString, QVariant> raw;
    for (auto it = m_overrides.constBegin(); it != m_overrides.constEnd(); ++it) {
        const Command* cmd = m_commands->command(it.key());
        if (cmd && cmd->defaultShortcut == it.value()) continue;   // equals default
        raw.insert(it.key(), it.value().toString());
    }
    m_settings->set(QStringLiteral("keyboard.overrides"), raw);
}

}  // namespace cf
