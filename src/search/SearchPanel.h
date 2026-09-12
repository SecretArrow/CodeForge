#pragma once
// Search sidebar (Ctrl+Shift+F): options + results tree + replace controls.
#include <QWidget>

#include "search/SearchEngine.h"

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace cf {

class SearchEngine;
class Workspace;
class DocumentManager;

using cf::FileResult;

class SearchPanel : public QWidget {
    Q_OBJECT
public:
    explicit SearchPanel(Workspace* workspace, QWidget* parent = nullptr);

    void openAndFocus();                 // also invoked by Ctrl+Shift+F

signals:
    void resultActivated(const QString& path, int line, int col);
    void fileReplacedOnDisk(const QString& path);   // reload open docs if needed

private slots:
    void runSearch();
    void onFileResult(const cf::FileResult& fr);
    void onFinished(const cf::SearchStats& stats);
    void replaceAll();

private:
    SearchQuery currentQuery() const;
    void populateGroup(const cf::FileResult& fr);

    Workspace* m_workspace;
    SearchEngine* m_engine;

    QLineEdit* m_search;
    QLineEdit* m_replace;
    QCheckBox* m_case;
    QCheckBox* m_word;
    QCheckBox* m_regex;
    QCheckBox* m_hidden;
    QLineEdit* m_include;
    QLineEdit* m_exclude;
    QTreeWidget* m_results;
    QPushButton* m_replaceBtn;
    QTreeWidgetItem* m_currentGroup = nullptr;
    int m_totalMatches = 0;
    int m_totalFiles = 0;
};

}  // namespace cf
