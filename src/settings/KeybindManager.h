#pragma once
// Keyboard shortcut manager: per-command overrides, conflict detection.
#include <QHash>
#include <QKeySequence>
#include <QObject>
#include <QStringList>

namespace cf {

class CommandRegistry;
class SettingsManager;

class KeybindManager : public QObject {
    Q_OBJECT
public:
    explicit KeybindManager(CommandRegistry* commands, SettingsManager* settings, QObject* parent = nullptr);

    void load();                        // read overrides from settings
    QKeySequence effective(const QString& commandId) const;

    // Set override (empty sequence clears override). Returns false on conflict.
    bool setBinding(const QString& commandId, const QKeySequence& seq, QString* conflictWith = nullptr);

    // Commands that currently own this sequence (excluding exceptId).
    QStringList conflicts(const QKeySequence& seq, const QString& exceptId = QString()) const;

    bool isOverridden(const QString& commandId) const;

signals:
    void bindingsChanged();

private:
    void persist();

    CommandRegistry* m_commands;
    SettingsManager* m_settings;
    QHash<QString, QKeySequence> m_overrides;
};

}  // namespace cf
