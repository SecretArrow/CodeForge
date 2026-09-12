#pragma once
// Settings manager: user settings (global) + workspace settings (override),
// declarative schema drives the Settings UI, import/export/reset supported.
// Stored as JSON; on Windows in %LOCALAPPDATA%, or portable/data in portable mode.
#include <QJsonObject>
#include <QObject>
#include <QVariant>
#include <QVector>

namespace cf {

struct SettingDef {
    enum class Type { Bool, Int, Double, String, Enum, StringList };
    QString key;
    QString page;      // Editor, Appearance, Files, Search, Terminal, Keyboard, Security, Workspace, Extensions, Performance
    QString label;
    QString tooltip;
    Type type = Type::Bool;
    QVariant defaultValue;
    // Enum choices: label -> value
    QVector<QPair<QString, QVariant>> choices;
    int minValue = 0;
    int maxValue = 1000000;
    double minDouble = 0.0;
    double maxDouble = 100.0;
};

class SettingsManager : public QObject {
    Q_OBJECT
public:
    static SettingsManager& instance();

    explicit SettingsManager(QObject* parent = nullptr);

    void load();

    // Workspace root for per-workspace overrides (empty = user scope only).
    void setWorkspaceRoot(const QString& root);

    // Value lookup: workspace override > user > schema default.
    QVariant get(const QString& key) const;
    bool getBool(const QString& key) const { return get(key).toBool(); }
    int getInt(const QString& key) const { return get(key).toInt(); }
    QString getString(const QString& key) const { return get(key).toString(); }

    enum class Scope { User, Workspace };
    void set(const QString& key, const QVariant& value, Scope scope = Scope::User);

    void resetAll();
    bool exportTo(const QString& file, QString* error = nullptr) const;
    bool importFrom(const QString& file, QString* error = nullptr);

    // Schema for the settings UI.
    const QVector<SettingDef>& defs() const { return m_defs; }
    const SettingDef* def(const QString& key) const;

    QString dataFile() const;

signals:
    // Emitted for each key that changed value (after load or set).
    void changed(const QString& key, const QVariant& value);

private:
    void buildSchema();
    void save(Scope scope);
    QVariant lookup(const QJsonObject& obj, const QString& key, bool* found) const;
    QJsonObject readJson(const QString& file) const;

    QJsonObject m_user;
    QJsonObject m_workspace;
    QString m_workspaceRoot;
    QVector<SettingDef> m_defs;
};

}  // namespace cf
