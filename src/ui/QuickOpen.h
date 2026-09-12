#pragma once
// Unified quick launcher popup:
//   Ctrl+P        -> files (fuzzy)
//   Ctrl+Shift+P  -> commands (fuzzy)
//   Ctrl+G        -> go to line
//   Ctrl+Shift+O  -> symbols in current document
#include <QWidget>

class QLineEdit;
class QListWidget;
class QListWidgetItem;

namespace cf {

class CommandRegistry;
class KeybindManager;
class Workspace;
class TextDocument;

class QuickOpen : public QWidget {
    Q_OBJECT
public:
    enum class Mode { Files, Commands, GotoLine, Symbols };

    explicit QuickOpen(QWidget* parent = nullptr);

    void openFiles();
    void openCommands();
    void openGotoLine();
    void openSymbols();

signals:
    void openFileRequested(const QString& path, bool preview);
    void executeCommand(const QString& commandId);
    void gotoLineRequested(int line);           // 0-based
    void gotoSymbolRequested(int line);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void refreshList();
    void acceptItem(QListWidgetItem* item);

private:
    void showForMode(Mode mode);
    void positionOverOwner();

    QLineEdit* m_input;
    QListWidget* m_list;
    Mode m_mode = Mode::Files;
};

}  // namespace cf
