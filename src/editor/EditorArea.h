#pragma once
// EditorArea: manages the splitter tree of EditorGroups (VS Code-like splits),
// active-group tracking, cross-group tab drag, and session serialization.
#include <QJsonObject>
#include <QSplitter>
#include <QWidget>

namespace cf {

class EditorGroup;
class TextDocument;

class EditorArea : public QWidget {
    Q_OBJECT
public:
    explicit EditorArea(QWidget* parent = nullptr);

    void openDocument(TextDocument* doc, bool preview = false);
    EditorGroup* activeGroup() const { return m_active; }
    void setActiveGroup(EditorGroup* g);

    EditorGroup* splitGroup(EditorGroup* g, Qt::Orientation orientation, bool moveCurrentTab);
    void closeGroup(EditorGroup* g);
    QList<EditorGroup*> groups() const { return m_groups; }
    int groupCount() const { return m_groups.size(); }

    // session
    QJsonObject saveState() const;
    bool restoreState(const QJsonObject& state);

signals:
    void activeDocChanged(cf::TextDocument* doc);
    void openStateChanged(bool anyOpen);        // false -> show Welcome page
    void groupActivated(cf::EditorGroup* group);

private:
    void registerGroup(EditorGroup* g);
    void unregisterGroup(EditorGroup* g);
    QSplitter* rootSplitter() const { return m_root; }
    void collapseIfPossible(QSplitter* parent);

    QSplitter* m_root;
    QList<EditorGroup*> m_groups;
    EditorGroup* m_active = nullptr;
};

}  // namespace cf
