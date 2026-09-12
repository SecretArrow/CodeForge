#pragma once
// In-editor find/replace bar (Ctrl+F / Ctrl+H), VS Code style.
#include <QList>
#include <QWidget>

class QCheckBox;
class QLineEdit;
class QPushButton;
class QTextCursor;
class QToolButton;

namespace cf {

class CodeEditor;
class TextDocument;

class FindReplaceBar : public QWidget {
    Q_OBJECT
public:
    explicit FindReplaceBar(CodeEditor* editor, QWidget* parent = nullptr);

    // Rebind to the currently visible editor (called when tabs switch).
    void setEditor(CodeEditor* editor) { m_editor = editor; }

    void openFind();
    void openReplace();
    void findNext();
    void findPrevious();

protected:
    void keyPressEvent(QKeyEvent* e) override;
    void showEvent(QShowEvent* e) override;

private slots:
    void refreshMatches();
    void replaceOne();
    void replaceAll();

private:
    void collectMatches();
    void jumpTo(int index);
    void updateCountLabel();

    CodeEditor* m_editor;
    QLineEdit* m_findEdit;
    QLineEdit* m_replaceEdit;
    QToolButton* m_caseBtn;
    QToolButton* m_wordBtn;
    QToolButton* m_regexBtn;
    QToolButton* m_prevBtn;
    QToolButton* m_nextBtn;
    QPushButton* m_replaceOneBtn;
    QPushButton* m_replaceAllBtn;
    QPushButton* m_replaceToggle;

    QList<QTextCursor> m_matches;
    int m_current = -1;
};

}  // namespace cf
