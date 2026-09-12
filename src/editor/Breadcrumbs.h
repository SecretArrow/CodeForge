#pragma once
// Breadcrumbs: workspace-relative path of the active file + nearest symbol.
#include <QWidget>

class QHBoxLayout;

namespace cf {

class TextDocument;
class Workspace;
struct SymbolInfo;

class Breadcrumbs : public QWidget {
    Q_OBJECT
public:
    explicit Breadcrumbs(QWidget* parent = nullptr);

    static void setWorkspace(cf::Workspace* ws) { s_workspace = ws; }

    void setDocument(cf::TextDocument* doc);           // nullptr clears
    void updateCursorLine(int line);

private slots:
    void rebuild();

private:
    QHBoxLayout* m_layout;
    cf::TextDocument* m_doc = nullptr;
    int m_line = 0;
    static Workspace* s_workspace;
};

}  // namespace cf
