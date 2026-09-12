#include "tools/DatabasePanel.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTableWidget>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "core/FileUtils.h"
#include "core/Logger.h"
#include "syntax/SyntaxHighlighter.h"

namespace cf {

DatabasePanel::DatabasePanel(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
}

void DatabasePanel::buildUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    auto* top = new QHBoxLayout;
    auto* openBtn = new QToolButton(this);
    openBtn->setText(tr("Open SQLite…"));
    auto* closeBtn = new QToolButton(this);
    closeBtn->setText(tr("Close"));
    closeBtn->setEnabled(false);
    m_connLabel = new QLabel(tr("No database open. Qt SQLITE driver is built into CodeForge."), this);
    top->addWidget(openBtn);
    top->addWidget(closeBtn);
    top->addWidget(m_connLabel, 1);
    layout->addLayout(top);

    auto* split = new QSplitter(Qt::Horizontal, this);
    layout->addWidget(split, 1);

    m_schema = new QTreeWidget(split);
    m_schema->setHeaderLabels({ tr("Table / Column") });
    m_schema->setMinimumWidth(220);

    auto* right = new QWidget(split);
    auto* rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(4);

    m_sql = new QPlainTextEdit(right);
    QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    m_sql->setFont(mono);
    m_sql->setPlaceholderText(QStringLiteral("SELECT * FROM table_name LIMIT 100;  (Ctrl+Enter runs)"));
    auto* hl = new SyntaxHighlighter(m_sql->document());
    hl->setLanguage(QStringLiteral("sql"));
    rightLayout->addWidget(m_sql, 1);

    auto* runRow = new QHBoxLayout;
    m_runBtn = new QToolButton(right);
    m_runBtn->setText(tr("Run (Ctrl+Enter)"));
    m_runBtn->setEnabled(false);
    m_exportBtn = new QToolButton(right);
    m_exportBtn->setText(tr("Export CSV"));
    m_exportBtn->setEnabled(false);
    m_resultMeta = new QLabel(right);
    runRow->addWidget(m_runBtn);
    runRow->addWidget(m_exportBtn);
    runRow->addWidget(m_resultMeta, 1);
    rightLayout->addLayout(runRow);

    m_results = new QTableWidget(0, 0, right);
    m_results->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_results->verticalHeader()->setVisible(false);
    rightLayout->addWidget(m_results, 2);

    split->addWidget(m_schema);
    split->addWidget(right);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 4);

    m_runBtn->setShortcut(QKeySequence(QStringLiteral("Ctrl+Return")));

    connect(openBtn, &QToolButton::clicked, this, &DatabasePanel::onOpenClicked);
    connect(closeBtn, &QToolButton::clicked, this, &DatabasePanel::onCloseClicked);
    connect(m_runBtn, &QToolButton::clicked, this, &DatabasePanel::onRunQuery);
    connect(m_exportBtn, &QToolButton::clicked, this, &DatabasePanel::onExportCsv);
    connect(m_schema, &QTreeWidget::itemDoubleClicked, this,
            [this](QTreeWidgetItem*, int) { onTableActivated(); });
    connect(m_schema, &QTreeWidget::itemActivated, this,
            [this](QTreeWidgetItem*, int) { onTableActivated(); });
}

void DatabasePanel::openDatabasePath(const QString& path)
{
    closeDatabase();
    if (path.isEmpty())
        return;
    const QString conn = QStringLiteral("cfdb-%1")
                             .arg(QDateTime::currentMSecsSinceEpoch());
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), conn);
    db.setDatabaseName(path);
    if (!db.open()) {
        QMessageBox::warning(this, tr("Database"),
                             tr("Cannot open %1:\n%2").arg(path, db.lastError().text()));
        return;
    }
    m_connectionName = conn;
    m_dbPath = path;
    m_connLabel->setText(tr("Connected: %1").arg(path));
    m_runBtn->setEnabled(true);
    loadSchema();
    Logger::instance().info(QStringLiteral("Database opened (%1 tables)")
                                .arg(db.tables().size()));
}

void DatabasePanel::onOpenClicked()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Open SQLite database"), QString(),
                                                      tr("SQLite database (*.db *.sqlite *.sqlite3 *.db3);;All files (*)"));
    if (path.isEmpty())
        return;
    openDatabasePath(path);
}

void DatabasePanel::closeDatabase()
{
    if (!m_connectionName.isEmpty()) {
        {
            QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
            if (db.isOpen())
                db.close();
        }
        QSqlDatabase::removeDatabase(m_connectionName);
    }
    m_connectionName.clear();
    m_dbPath.clear();
    m_schema->clear();
    m_results->setRowCount(0);
    m_results->setColumnCount(0);
    m_connLabel->setText(tr("No database open."));
    m_runBtn->setEnabled(false);
    m_exportBtn->setEnabled(false);
}

void DatabasePanel::loadSchema()
{
    m_schema->clear();
    if (m_connectionName.isEmpty())
        return;
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    const QStringList tables = db.tables();
    for (const QString& t : tables) {
        auto* tableItem = new QTreeWidgetItem(m_schema);
        tableItem->setText(0, t);
        tableItem->setIcon(0, QIcon());   // theme-colored via delegate later
        QSqlQuery cols(db);
        cols.exec(QStringLiteral("PRAGMA table_info(%1)").arg(t));
        while (cols.next()) {
            auto* col = new QTreeWidgetItem(tableItem);
            col->setText(0, QStringLiteral("%1  %2")
                                .arg(cols.value(1).toString(), cols.value(2).toString()));
        }
    }
    m_schema->expandAll();
}

void DatabasePanel::onTableActivated()
{
    QTreeWidgetItem* it = m_schema->currentItem();
    if (!it)
        return;
    QTreeWidgetItem* parent = it->parent();
    const QString table = parent ? parent->text(0) : it->text(0);
    if (table.isEmpty())
        return;
    m_sql->setPlainText(QStringLiteral("SELECT * FROM %1 LIMIT 200;").arg(table));
    onRunQuery();
}

void DatabasePanel::onRunQuery()
{
    if (m_connectionName.isEmpty())
        return;
    const QString sql = m_sql->toPlainText().trimmed();
    if (sql.isEmpty())
        return;
    m_lastQuery = sql;
    showRows(sql);
}

void DatabasePanel::showRows(const QString& sql)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    QElapsedTimer timer;
    timer.start();
    if (!q.exec(sql)) {
        m_resultMeta->setText(q.lastError().text());
        QMessageBox::warning(this, tr("Query error"), q.lastError().text());
        return;
    }
    m_lastElapsedMs = timer.elapsed();

    const QSqlRecord rec = q.record();
    const int cols = rec.count();
    m_results->clear();
    m_results->setColumnCount(cols);
    QStringList headers;
    for (int c = 0; c < cols; ++c)
        headers << rec.fieldName(c);
    m_results->setHorizontalHeaderLabels(headers);

    int row = 0;
    bool truncated = false;
    constexpr int kMaxRows = 5000;
    while (q.next()) {
        if (row >= kMaxRows) {
            truncated = true;
            break;
        }
        m_results->insertRow(row);
        for (int c = 0; c < cols; ++c)
            m_results->setItem(row, c, new QTableWidgetItem(q.value(c).toString()));
        ++row;
    }
    m_lastRowCount = row;
    m_resultMeta->setText(tr("%1 row(s)%2 · %3 ms")
                              .arg(row)
                              .arg(truncated ? tr(" (truncated at %1)").arg(kMaxRows) : QString())
                              .arg(m_lastElapsedMs));
    m_exportBtn->setEnabled(row > 0);
    m_results->resizeColumnsToContents();
}

void DatabasePanel::onExportCsv()
{
    if (m_results->rowCount() == 0)
        return;
    const QString path = QFileDialog::getSaveFileName(this, tr("Export CSV"),
                                                      QStringLiteral("results.csv"),
                                                      tr("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    QString csv;
    const int cols = m_results->columnCount();
    for (int c = 0; c < cols; ++c)
        csv += (c ? QStringLiteral(",") : QString())
               + QStringLiteral("\"%1\"").arg(m_results->horizontalHeaderItem(c)->text());
    csv += QStringLiteral("\n");
    for (int r = 0; r < m_results->rowCount(); ++r) {
        for (int c = 0; c < cols; ++c) {
            const QString v = m_results->item(r, c) ? m_results->item(r, c)->text() : QString();
            csv += (c ? QStringLiteral(",") : QString()) + QStringLiteral("\"%1\"").arg(v);
        }
        csv += QStringLiteral("\n");
    }
    QString err;
    if (!fs::writeAllAtomic(path, csv.toUtf8(), &err)) {
        QMessageBox::warning(this, tr("Export CSV"), err);
        return;
    }
    m_resultMeta->setText(tr("Exported %1 row(s) to %2").arg(m_results->rowCount()).arg(path));
}

}  // namespace cf
