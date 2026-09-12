#pragma once
// SnippetsDialog: manage user snippets (add / edit / remove), persisted to
// dataRoot()/snippets.json. Language list mirrors LanguageRegistry.
#include <QDialog>

class QListWidget;
class QLineEdit;
class QPlainTextEdit;
class QComboBox;

namespace cf {

class SnippetsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SnippetsDialog(QWidget* parent = nullptr);

private slots:
    void onSelectionChanged();
    void onFieldChanged();
    void onAdd();
    void onRemove();
    void onSave();

private:
    void reloadList(int selectRow = -1);
    void loadRow(int row);

    QListWidget* m_list;
    QLineEdit* m_trigger;
    QLineEdit* m_description;
    QLineEdit* m_languages;
    QPlainTextEdit* m_body;
    bool m_updating = false;
};

}  // namespace cf
