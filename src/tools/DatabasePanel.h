#pragma once
// DatabasePanel: SQLite browser and query runner (QtSql / QSQLITE driver).
// Open a database file, browse tables and columns, run arbitrary SQL,
// export results as CSV. Row display is capped for responsiveness.
#include <QWidget>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QTableWidget;
class QTreeWidget;
class QToolButton;

namespace cf {

class DatabasePanel : public QWidget {
    Q_OBJECT
public:
    explicit DatabasePanel(QWidget* parent = nullptr);

    void openDatabasePath(const QString& path);   // command entry + tests

private slots:
    void onOpenClicked();
    void onCloseClicked();
    void onRunQuery();
    void onTableActivated();
    void onExportCsv();

private:
    void buildUi();
    void loadSchema();
    void closeDatabase();
    void showRows(const QString& sql);

    QTreeWidget* m_schema;
    QPlainTextEdit* m_sql;
    QTableWidget* m_results;
    QLabel* m_connLabel;
    QLabel* m_resultMeta;
    QToolButton* m_runBtn;
    QToolButton* m_exportBtn;
    QString m_connectionName;
    QString m_dbPath;
    QString m_lastQuery;
    qint64 m_lastElapsedMs = 0;
    int m_lastRowCount = 0;
};

}  // namespace cf
