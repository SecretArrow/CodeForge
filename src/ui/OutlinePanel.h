#pragma once
// Outline sidebar: document symbols of the active editor.
#include <QWidget>

class QTreeWidget;
class QTreeWidgetItem;

namespace cf {

class EditorArea;

class OutlinePanel : public QWidget {
    Q_OBJECT
public:
    explicit OutlinePanel(EditorArea* editorArea, QWidget* parent = nullptr);

public slots:
    void refreshForDocument(class TextDocument* doc);
    void clearOutline();

private slots:
    void onSymbolActivated(QTreeWidgetItem* item, int col);

private:
    EditorArea* m_editorArea;
    QTreeWidget* m_tree;
    class TextDocument* m_doc = nullptr;
};

}  // namespace cf
