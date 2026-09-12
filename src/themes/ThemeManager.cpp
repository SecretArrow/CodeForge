#include "themes/ThemeManager.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QPalette>

#include "core/AppPaths.h"
#include "core/Logger.h"
#include "themes/BuiltinResources.h"
#include "settings/SettingsManager.h"

namespace cf {

ThemeManager& ThemeManager::instance()
{
    static ThemeManager s(&SettingsManager::instance());
    return s;
}

ThemeManager::ThemeManager(SettingsManager* settings, QObject* parent)
    : QObject(parent), m_settings(settings)
{
}

void ThemeManager::loadThemes()
{
    initBuiltinResources();
    m_themes.clear();

    // Built-in themes from resources.
    for (const QString& res : { QStringLiteral("dark-plus"), QStringLiteral("light-plus"), QStringLiteral("dracula"),
                                QStringLiteral("monokai"), QStringLiteral("hc-dark"), QStringLiteral("hc-light") }) {
        QFile f(QStringLiteral(":/themes/%1.json").arg(res));
        if (f.open(QIODevice::ReadOnly)) {
            Theme t;
            if (t.parse(f.readAll())) m_themes.append(t);
        }
    }

    // User themes from data dir.
    const QDir userDir(paths::themesDir());
    for (const QFileInfo& fi : userDir.entryInfoList({ QStringLiteral("*.json") }, QDir::Files)) {
        QFile f(fi.absoluteFilePath());
        if (!f.open(QIODevice::ReadOnly)) continue;
        Theme t;
        if (t.parse(f.readAll())) {
            bool replaced = false;
            for (Theme& existing : m_themes)
                if (existing.id() == t.id()) { existing = t; replaced = true; }
            if (!replaced) m_themes.append(t);
        } else {
            CF_LOG_WARNING(QStringLiteral("ThemeManager: invalid theme file %1").arg(fi.fileName()));
        }
    }

    if (m_themes.isEmpty()) {
        // Emergency fallback so the app always has a theme.
        Theme fallback;
        fallback.parse(QByteArrayLiteral("{\"id\":\"fallback\",\"name\":\"Fallback Dark\",\"type\":\"dark\",\"colors\":{\"editor.background\":\"#1e1e1e\",\"editor.foreground\":\"#d4d4d4\"}}"));
        m_themes.append(fallback);
    }
}

Theme ThemeManager::themeById(const QString& id) const
{
    for (const Theme& t : m_themes)
        if (t.id() == id) return t;
    for (const Theme& t : m_themes)
        if (t.isDark()) return t;
    return m_themes.first();
}

Theme ThemeManager::currentTheme() const
{
    return themeById(m_settings ? m_settings->get(QStringLiteral("appearance.theme")).toString() : QString());
}

void ThemeManager::setTheme(const QString& id)
{
    if (m_settings) m_settings->set(QStringLiteral("appearance.theme"), id);
    const Theme t = themeById(id);
    applyTheme(t);
}

void ThemeManager::applyTheme(const Theme& t)
{
    QPalette pal;

    const QColor bg = t.color(QStringLiteral("ui.background"));
    const QColor fg = t.color(QStringLiteral("ui.foreground"));
    const QColor disabled = t.color(QStringLiteral("ui.disabledText"));
    const QColor base = t.editorBackground();
    const QColor text = t.editorForeground();
    const QColor accent = t.color(QStringLiteral("ui.accent"));

    pal.setColor(QPalette::Window, bg);
    pal.setColor(QPalette::WindowText, fg);
    pal.setColor(QPalette::Base, base);
    pal.setColor(QPalette::AlternateBase, t.color(QStringLiteral("ui.sidebar")));
    pal.setColor(QPalette::Text, text);
    pal.setColor(QPalette::PlaceholderText, disabled);
    pal.setColor(QPalette::Button, t.color(QStringLiteral("ui.tab.inactive")));
    pal.setColor(QPalette::ButtonText, fg);
    pal.setColor(QPalette::ToolTipBase, t.color(QStringLiteral("ui.tooltip")));
    pal.setColor(QPalette::ToolTipText, fg);
    pal.setColor(QPalette::BrightText, Qt::white);
    pal.setColor(QPalette::Highlight, t.color(QStringLiteral("ui.list.selected")));
    pal.setColor(QPalette::HighlightedText, fg);
    pal.setColor(QPalette::Link, t.color(QStringLiteral("ui.link")));
    for (QPalette::ColorGroup g : { QPalette::Disabled }) {
        pal.setColor(g, QPalette::WindowText, disabled);
        pal.setColor(g, QPalette::Text, disabled);
        pal.setColor(g, QPalette::ButtonText, disabled);
    }
    QGuiApplication::setPalette(pal);
    if (qApp) qApp->setStyleSheet(appStyleSheet(t));

    emit themeChanged(t);
}

QString ThemeManager::appStyleSheet(const Theme& t) const
{
    const QString border = t.color(QStringLiteral("ui.border")).name();
    const QString hover = t.color(QStringLiteral("ui.list.hover")).name();
    const QString selected = t.color(QStringLiteral("ui.list.selected")).name();
    const QString accent = t.color(QStringLiteral("ui.accent")).name();
    const QString input = t.color(QStringLiteral("ui.input")).name();
    const QString inputBorder = t.color(QStringLiteral("ui.inputBorder")).name();

    return QStringLiteral(R"(
QMainWindow::separator { background: %border%; width: 4px; height: 4px; }
QSplitter::handle { background: %border%; }
QSplitter::handle:horizontal { width: 1px; }
QSplitter::handle:vertical { height: 1px; }
QToolBar { background: %bg%; border: none; spacing: 2px; padding: 2px; }
QStatusBar { background: %statusbar%; color: %statusbarText%; border: none; }
QStatusBar QLabel, QStatusBar QToolButton { background: transparent; color: %statusbarText%; border: none; padding: 1px 6px; }
QStatusBar QToolButton:hover { background: rgba(255,255,255,40); }
QMenuBar { background: %bg%; color: %fg%; border-bottom: 1px solid %border%; }
QMenuBar::item:selected { background: %hover%; }
QMenu { background: %sidebar%; color: %fg%; border: 1px solid %border%; }
QMenu::item:selected { background: %selected%; }
QMenu::separator { height: 1px; background: %border%; margin: 4px 8px; }
QTabBar::tab { background: %tabInactive%; color: %fg%; padding: 5px 10px; border: 1px solid %border%; border-bottom: none; margin-right: 1px; }
QTabBar::tab:selected { background: %tabActive%; border-top: 2px solid %accent%; }
QTabBar::tab:hover { background: %hover%; }
QTabWidget::pane { border: 1px solid %border%; }
QTreeView, QListView, QTableView { background: %sidebar%; color: %fg%; border: none; outline: 0; }
QTreeView::item, QListView::item { padding: 2px 4px; border: none; }
QTreeView::item:hover, QListView::item:hover { background: %hover%; }
QTreeView::item:selected, QListView::item:selected { background: %selected%; color: %fg%; }
QHeaderView::section { background: %sidebar%; color: %fg%; border: none; border-right: 1px solid %border%; border-bottom: 1px solid %border%; padding: 4px; }
QLineEdit, QPlainTextEdit, QTextEdit, QSpinBox, QDoubleSpinBox, QComboBox {
    background: %input%; color: %fg%; border: 1px solid %inputBorder%; padding: 3px 5px; selection-background-color: %selected%;
}
QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus { border: 1px solid %accent%; }
QComboBox::drop-down { border: none; width: 18px; }
QComboBox QAbstractItemView { background: %sidebar%; color: %fg%; selection-background-color: %selected%; }
QPushButton { background: %button%; color: %buttonText%; border: none; padding: 5px 14px; }
QPushButton:hover { background: %accent%; }
QPushButton:disabled { background: %input%; color: %disabled%; }
QPushButton:flat, QToolButton { background: transparent; color: %fg%; border: none; padding: 3px; }
QToolButton:hover { background: %hover%; border-radius: 3px; }
QToolButton:checked { background: %selected%; }
QScrollBar:vertical { background: transparent; width: 12px; margin: 0; }
QScrollBar::handle:vertical { background: %inputBorder%; min-height: 30px; border-radius: 4px; margin: 2px; }
QScrollBar::handle:vertical:hover { background: %fg%; }
QScrollBar:horizontal { background: transparent; height: 12px; margin: 0; }
QScrollBar::handle:horizontal { background: %inputBorder%; min-width: 30px; border-radius: 4px; margin: 2px; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
QToolTip { background: %tooltip%; color: %fg%; border: 1px solid %border%; padding: 4px; }
QGroupBox { border: 1px solid %border%; margin-top: 10px; padding-top: 6px; }
QGroupBox::title { subcontrol-origin: margin; left: 8px; color: %fg%; }
QCheckBox, QRadioButton, QLabel { color: %fg%; }
QListWidget { background: %sidebar%; color: %fg%; border: none; }
QDialog { background: %bg%; }
)")
        .replace(QStringLiteral("%border%"), border)
        .replace(QStringLiteral("%bg%"), t.color(QStringLiteral("ui.background")).name())
        .replace(QStringLiteral("%fg%"), t.color(QStringLiteral("ui.foreground")).name())
        .replace(QStringLiteral("%sidebar%"), t.color(QStringLiteral("ui.sidebar")).name())
        .replace(QStringLiteral("%statusbar%"), t.color(QStringLiteral("ui.statusBar")).name())
        .replace(QStringLiteral("%statusbarText%"), t.color(QStringLiteral("ui.statusBarText")).name())
        .replace(QStringLiteral("%tabInactive%"), t.color(QStringLiteral("ui.tab.inactive")).name())
        .replace(QStringLiteral("%tabActive%"), t.color(QStringLiteral("ui.tab.active")).name())
        .replace(QStringLiteral("%hover%"), hover)
        .replace(QStringLiteral("%selected%"), selected)
        .replace(QStringLiteral("%accent%"), accent)
        .replace(QStringLiteral("%input%"), input)
        .replace(QStringLiteral("%inputBorder%"), inputBorder)
        .replace(QStringLiteral("%button%"), t.color(QStringLiteral("ui.button")).name())
        .replace(QStringLiteral("%buttonText%"), t.color(QStringLiteral("ui.buttonText")).name())
        .replace(QStringLiteral("%disabled%"), t.color(QStringLiteral("ui.disabledText")).name())
        .replace(QStringLiteral("%tooltip%"), t.color(QStringLiteral("ui.tooltip")).name());
}

}  // namespace cf
