#pragma once
// Extension host: scans the extensions folder, loads native plugins via
// QPluginLoader, tracks metadata for the Extensions view.
#include <QObject>
#include <QVector>

#include "extensions/IExtension.h"

namespace cf {

struct AppContext;

class ExtensionHost : public QObject {
    Q_OBJECT
public:
    struct Entry {
        QString id, name, version, description;
        QString sourceFile;      // plugin file, empty for built-ins
        bool loaded = false;
        QString error;
    };

    explicit ExtensionHost(QObject* parent = nullptr);

    // Scan + load plugins. Built-in data packs (themes) are reported too.
    void loadAll(const AppContext& context);
    const QVector<Entry>& entries() const { return m_entries; }
    QVector<IExtension*> activePlugins() const { return m_plugins; }

    QString extensionsFolder() const;

signals:
    void loadFinished(int pluginsLoaded, const QString& folder);

private:
    void scanFolder(const QString& folder, const AppContext& context);

    QVector<Entry> m_entries;
    QVector<IExtension*> m_plugins;
};

}  // namespace cf
