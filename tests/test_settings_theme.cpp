#include <QtTest>

#include "settings/SettingsManager.h"
#include "themes/Theme.h"
#include "themes/ThemeManager.h"

using namespace cf;

class TestSettingsTheme : public QObject {
    Q_OBJECT
private slots:
    void defaultsExist()
    {
        SettingsManager& s = SettingsManager::instance();
        QCOMPARE(s.getString(QStringLiteral("appearance.theme")), QStringLiteral("dark-plus"));
        QCOMPARE(s.getInt(QStringLiteral("editor.tabSize")), 4);
        QCOMPARE(s.getBool(QStringLiteral("editor.insertSpaces")), true);
        QCOMPARE(s.getString(QStringLiteral("files.autosave")), QStringLiteral("off"));
    }

    void setGetRoundTrip()
    {
        SettingsManager& s = SettingsManager::instance();
        s.set(QStringLiteral("editor.tabSize"), 8);
        QCOMPARE(s.getInt(QStringLiteral("editor.tabSize")), 8);
        s.set(QStringLiteral("editor.tabSize"), 4);   // restore
    }

    void themeParsing()
    {
        Theme t;
        QVERIFY(t.parse(QByteArrayLiteral(
            "{\"id\":\"test\",\"name\":\"Test\",\"type\":\"dark\","
            "\"colors\":{\"editor.background\":\"#101010\",\"editor.foreground\":\"#eeeeee\"}}")));
        QCOMPARE(t.id(), QStringLiteral("test"));
        QVERIFY(t.isDark());
        QCOMPARE(t.editorBackground(), QColor(0x10, 0x10, 0x10));
        // Token fallback derives from foreground.
        QCOMPARE(t.syntaxColor(QStringLiteral("keyword")), QColor(0xee, 0xee, 0xee));
    }

    void invalidThemeRejected()
    {
        Theme t;
        QVERIFY(!t.parse(QByteArrayLiteral("{not json")));
        QVERIFY(!t.parse(QByteArrayLiteral("{\"name\":\"no id\"}")));
    }

    void builtinThemesLoad()
    {
        ThemeManager& tm = ThemeManager::instance();
        tm.loadThemes();
        QVERIFY(tm.themes().size() >= 6);
        QVERIFY(!tm.themeById(QStringLiteral("dracula")).isDark() == false);   // dracula is dark
        QCOMPARE(tm.themeById(QStringLiteral("light-plus")).isDark(), false);
    }
};

QTEST_GUILESS_MAIN(TestSettingsTheme)
#include "test_settings_theme.moc"
