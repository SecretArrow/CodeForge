#pragma once
// Central command registry: every action in the app is registered here so the
// Command Palette, keyboard shortcut manager and menus share one source of truth.
#include <QHash>
#include <QKeySequence>
#include <QObject>
#include <QString>
#include <functional>

namespace cf {

struct Command {
    QString id;                 // e.g. "file.save"
    QString title;              // e.g. "File: Save"
    QString category;           // e.g. "File"
    QKeySequence defaultShortcut;
    std::function<void()> run;
};

class CommandRegistry : public QObject {
    Q_OBJECT
public:
    explicit CommandRegistry(QObject* parent = nullptr);

    void registerCommand(const QString& id, const QString& category, const QString& title,
                         const QKeySequence& defaultShortcut, std::function<void()> fn);

    bool execute(const QString& id) const;
    const Command* command(const QString& id) const;
    QList<const Command*> commands() const;   // sorted by category/title

private:
    QHash<QString, Command> m_commands;
};

}  // namespace cf
