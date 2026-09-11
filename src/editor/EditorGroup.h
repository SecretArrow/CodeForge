#pragma once
// EditorGroup: one tab bar + one editor stack. Multiple groups live in a
// splitter tree managed by EditorArea (VS Code-like splits).
#include <QWidget>
#include <QVector>

class QStackedWidget;
class QJsonArray;

namespace cf {

class CodeEditor;
class FindReplaceBar;
class TabBar;
class TextDocument;

class EditorGroup : public QWidget {
    Q_OBJECT
public:
    explicit EditorGroup(QWidget* parent = nullptr);

    void openDocument(TextDocument* doc, bool preview = false);
    void activateDocument(TextDocument* doc);
    TextDocument* currentDocument() const;
    CodeEditor* currentEditor() const;
    CodeEditor* editorForDoc(TextDocument* doc) const;
    bool containsDoc(TextDocument* doc) const;
    int indexOfDoc(TextDocument* doc) const;
    int tabCount() const { return m_docs.size(); }
    QList<TextDocument*> documents() const { return m_docs; }

    bool closeTab(int index);        // false if user cancelled
    void closeOtherTabs(int index);
    void closeAllTabs();
    void closeSavedTabs();
    void nextTab();
    void previousTab();
    void togglePin(int index);
    void moveTab(int from, int to);
    void insertTabAt(TextDocument* doc, int index);

    void showFind(bool replace);
    FindReplaceBar* findBar() const { return m_find; }

    // session
    QJsonArray saveState() const;

signals:
    void currentDocChanged(cf::TextDocument* doc);
    void becameEmpty(cf::EditorGroup* group);
    void focusActivated(cf::EditorGroup* group);
    void statusMessage(const QString& msg);

public slots:
    void refreshTabs();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    CodeEditor* createEditor(TextDocument* doc);
    void connectDocSignals(TextDocument* doc);
    void activateTab(int index);
    void promotePreview(int index);
    int m_previewIndex = -1;

    TabBar* m_tabs;
    QStackedWidget* m_stack;
    FindReplaceBar* m_find;
    QList<TextDocument*> m_docs;
    QVector<bool> m_pinned;
    int m_current = -1;
};

}  // namespace cf
