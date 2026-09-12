#pragma once
// Settings dialog: schema-driven pages, keyboard editor, import/export/reset.
#include <QDialog>

class QComboBox;
class QLineEdit;
class QListWidget;
class QStackedWidget;
class QSpinBox;
class QDoubleSpinBox;
class QTableWidget;

namespace cf {

class SettingsManager;
class KeybindManager;
class CommandRegistry;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(SettingsManager* settings, KeybindManager* keybinds,
                            CommandRegistry* commands, QWidget* parent = nullptr);

private slots:
    void onPageChanged(int row);
    void applyPendingEdits();
    void exportSettings();
    void importSettings();
    void resetSettings();

private:
    QWidget* buildSchemaPage(const QString& page);
    QWidget* buildKeyboardPage();
    void populateKeyboardTable();

    SettingsManager* m_settings;
    KeybindManager* m_keybinds;
    CommandRegistry* m_commands;

    QListWidget* m_categories;
    QStackedWidget* m_pages;
    QTableWidget* m_shortcutsTable;

    // live-edit widgets per key (applied on change)
    struct EditorWidgets {
        QComboBox* combo = nullptr;
        QSpinBox* spin = nullptr;
        QDoubleSpinBox* dspin = nullptr;
        QLineEdit* edit = nullptr;
        QString key;
    };
    QList<EditorWidgets> m_editors;
};

}  // namespace cf
