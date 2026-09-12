#pragma once
// TodoPanel: sidebar page listing TODO/FIXME/HACK/XXX markers across the
// workspace (real background search via SearchEngine, grouped per file).
#include <QWidget>

#include "search/SearchEngine.h"

class QLabel;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace cf {

class Workspace;

class TodoPanel : public QWidget {
    Q_OBJECT
public:
    explicit TodoPanel(Workspace* workspace, QWidget* parent = nullptr);

    void refresh();          // run scan now
    void clearResults();

signals:
    void resultActivated(const QString& path, int line, int col);

private:
    void onFileResult(const cf::FileResult& result);
    void onFinished(const cf::SearchStats& stats);

    Workspace* m_workspace;
    SearchEngine m_engine;
    QTreeWidget* m_tree;
    QLabel* m_summary;
    int m_total = 0;
    int m_files = 0;
};

}  // namespace cf
