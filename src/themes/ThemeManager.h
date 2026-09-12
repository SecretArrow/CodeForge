#pragma once
// Loads built-in themes (Qt resources) + user themes (themes dir),
// applies them via QPalette + application stylesheet, notifies listeners.
#include <QObject>
#include <QVector>

#include "themes/Theme.h"

namespace cf {

class SettingsManager;

class ThemeManager : public QObject {
    Q_OBJECT
public:
    static ThemeManager& instance();

    explicit ThemeManager(SettingsManager* settings, QObject* parent = nullptr);

    void loadThemes();
    QVector<Theme> themes() const { return m_themes; }
    Theme themeById(const QString& id) const;      // falls back to first dark theme
    Theme currentTheme() const;

    void setTheme(const QString& id);              // persists setting + applies
    void applyTheme(const Theme& theme);           // palette + stylesheet + repaint

signals:
    void themeChanged(const cf::Theme& theme);

private:
    QString appStyleSheet(const Theme& t) const;

    SettingsManager* m_settings;
    QVector<Theme> m_themes;
};

}  // namespace cf
