#include "settings/SettingsDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

#include "core/CommandRegistry.h"
#include "settings/KeybindManager.h"
#include "settings/SettingsManager.h"

namespace cf {

// Captures a key sequence from focused key events.
class KeySequenceEdit : public QLineEdit {
public:
    explicit KeySequenceEdit(QWidget* parent) : QLineEdit(parent) { setReadOnly(true); }
    void setSequence(const QKeySequence& s) { setText(s.toString()); }
    QKeySequence sequence() const { return QKeySequence(text()); }

protected:
    void keyPressEvent(QKeyEvent* e) override
    {
        const int key = e->key();
        if (key == Qt::Key_Escape) { clearFocus(); return; }
        if (key == Qt::Key_Backspace && e->modifiers() == Qt::NoModifier) { clear(); return; }
        const int mods = e->modifiers() & ~Qt::KeypadModifier;
        if (key == Qt::Key_unknown || key == 0) return;
        setText(QKeySequence(static_cast<int>(mods | key)).toString());
    }
};

SettingsDialog::SettingsDialog(SettingsManager* settings, KeybindManager* keybinds,
                               CommandRegistry* commands, QWidget* parent)
    : QDialog(parent, Qt::Window), m_settings(settings), m_keybinds(keybinds), m_commands(commands)
{
    setWindowTitle(tr("Settings"));
    resize(860, 620);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_categories = new QListWidget(this);
    m_categories->setFixedWidth(180);
    for (const QString& page : { tr("Editor"), tr("Appearance"), tr("Files"), tr("Search"), tr("Terminal"),
                                 tr("Keyboard"), tr("Security"), tr("Workspace"), tr("Extensions"),
                                 tr("Performance"), tr("Updates") })
        m_categories->addItem(page);
    layout->addWidget(m_categories);

    m_pages = new QStackedWidget(this);
    layout->addWidget(m_pages, 1);

    m_pages->addWidget(buildSchemaPage(QStringLiteral("Editor")));
    m_pages->addWidget(buildSchemaPage(QStringLiteral("Appearance")));
    m_pages->addWidget(buildSchemaPage(QStringLiteral("Files")));
    m_pages->addWidget(buildSchemaPage(QStringLiteral("Search")));
    m_pages->addWidget(buildSchemaPage(QStringLiteral("Terminal")));
    m_pages->addWidget(buildKeyboardPage());
    m_pages->addWidget(buildSchemaPage(QStringLiteral("Security")));
    m_pages->addWidget(buildSchemaPage(QStringLiteral("Workspace")));
    m_pages->addWidget(buildSchemaPage(QStringLiteral("Extensions")));
    m_pages->addWidget(buildSchemaPage(QStringLiteral("Performance")));
    m_pages->addWidget(buildSchemaPage(QStringLiteral("Updates")));

    connect(m_categories, &QListWidget::currentRowChanged, this, &SettingsDialog::onPageChanged);
    m_categories->setCurrentRow(0);

    auto* footer = new QHBoxLayout();
    footer->setContentsMargins(12, 8, 12, 8);
    auto* exportBtn = new QPushButton(tr("Export..."), this);
    auto* importBtn = new QPushButton(tr("Import..."), this);
    auto* resetBtn = new QPushButton(tr("Reset All"), this);
    connect(exportBtn, &QPushButton::clicked, this, &SettingsDialog::exportSettings);
    connect(importBtn, &QPushButton::clicked, this, &SettingsDialog::importSettings);
    connect(resetBtn, &QPushButton::clicked, this, &SettingsDialog::resetSettings);
    footer->addWidget(exportBtn);
    footer->addWidget(importBtn);
    footer->addWidget(resetBtn);
    footer->addStretch(1);
    auto* closeBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(closeBox, &QDialogButtonBox::rejected, this, &QDialog::accept);
    footer->addWidget(closeBox);

    auto* outer = new QVBoxLayout(this);
    outer->addLayout(layout, 1);
    outer->addLayout(footer);
}

void SettingsDialog::onPageChanged(int row)
{
    m_pages->setCurrentIndex(row);
}

QWidget* SettingsDialog::buildSchemaPage(const QString& page)
{
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameStyle(QFrame::NoFrame);
    auto* host = new QWidget(scroll);
    auto* form = new QFormLayout(host);
    form->setLabelAlignment(Qt::AlignLeft);

    for (const SettingDef& def : m_settings->defs()) {
        if (def.page != page) continue;

        QLabel* label = new QLabel(def.label, host);
        label->setToolTip(def.tooltip);
        QWidget* field = nullptr;

        switch (def.type) {
        case SettingDef::Type::Bool: {
            auto* check = new QCheckBox(host);
            check->setChecked(m_settings->get(def.key).toBool());
            check->setToolTip(def.tooltip);
            connect(check, &QCheckBox::toggled, this, [this, def](bool v) {
                m_settings->set(def.key, v);
            });
            field = check;
            break;
        }
        case SettingDef::Type::Int: {
            auto* spin = new QSpinBox(host);
            spin->setRange(def.minValue, def.maxValue);
            spin->setValue(m_settings->get(def.key).toInt());
            connect(spin, &QSpinBox::valueChanged, this, [this, def](int v) {
                m_settings->set(def.key, v);
            });
            field = spin;
            break;
        }
        case SettingDef::Type::Double: {
            auto* spin = new QDoubleSpinBox(host);
            spin->setRange(def.minDouble, def.maxDouble);
            spin->setSingleStep(0.05);
            spin->setValue(m_settings->get(def.key).toDouble());
            connect(spin, &QDoubleSpinBox::valueChanged, this, [this, def](double v) {
                m_settings->set(def.key, v);
            });
            field = spin;
            break;
        }
        case SettingDef::Type::Enum: {
            auto* combo = new QComboBox(host);
            for (const auto& [labelText, value] : def.choices)
                combo->addItem(labelText, value);
            const QVariant current = m_settings->get(def.key);
            const int idx = combo->findData(current);
            combo->setCurrentIndex(idx >= 0 ? idx : 0);
            connect(combo, &QComboBox::currentIndexChanged, this, [this, def, combo](int) {
                m_settings->set(def.key, combo->currentData());
            });
            field = combo;
            break;
        }
        case SettingDef::Type::StringList: {
            auto* edit = new QLineEdit(host);
            edit->setText(m_settings->get(def.key).toStringList().join(QStringLiteral(", ")));
            edit->setToolTip(def.tooltip + QStringLiteral("\n") + tr("Comma-separated list."));
            connect(edit, &QLineEdit::editingFinished, this, [this, def, edit]() {
                QStringList items = edit->text().split(QLatin1Char(','), Qt::SkipEmptyParts);
                for (QString& s : items) s = s.trimmed();
                m_settings->set(def.key, items);
            });
            field = edit;
            break;
        }
        case SettingDef::Type::String:
        default: {
            auto* edit = new QLineEdit(host);
            edit->setText(m_settings->get(def.key).toString());
            connect(edit, &QLineEdit::editingFinished, this, [this, def, edit]() {
                m_settings->set(def.key, edit->text());
            });
            field = edit;
            break;
        }
        }

        form->addRow(label, field);
    }

    scroll->setWidget(host);
    return scroll;
}

QWidget* SettingsDialog::buildKeyboardPage()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->addWidget(new QLabel(tr("Double-click a shortcut to change it. Conflicts are rejected."), page));

    m_shortcutsTable = new QTableWidget(page);
    m_shortcutsTable->setColumnCount(3);
    m_shortcutsTable->setHorizontalHeaderLabels({ tr("Command"), tr("Shortcut"), tr("Category") });
    m_shortcutsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_shortcutsTable->verticalHeader()->hide();
    m_shortcutsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_shortcutsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_shortcutsTable->setSortingEnabled(false);
    layout->addWidget(m_shortcutsTable, 1);
    populateKeyboardTable();

    return page;
}

void SettingsDialog::populateKeyboardTable()
{
    const QList<const Command*> cmds = m_commands->commands();
    m_shortcutsTable->setRowCount(int(cmds.size()));
    for (int i = 0; i < cmds.size(); ++i) {
        const Command* c = cmds.at(i);
        auto* name = new QTableWidgetItem(QStringLiteral("%1: %2").arg(c->category, c->title));
        name->setData(Qt::UserRole, c->id);
        auto* seq = new QTableWidgetItem(m_keybinds->effective(c->id).toString());
        seq->setToolTip(tr("Click, then press a key combination (Esc cancels)"));
        auto* cat = new QTableWidgetItem(c->category);
        m_shortcutsTable->setItem(i, 0, name);
        m_shortcutsTable->setItem(i, 1, seq);
        m_shortcutsTable->setItem(i, 2, cat);
    }

    // Capture-on-click editing of the shortcut cell.
    disconnect(m_shortcutsTable, &QTableWidget::cellDoubleClicked, nullptr, nullptr);
    connect(m_shortcutsTable, &QTableWidget::cellDoubleClicked, this, [this](int row, int col) {
        if (col != 1) return;
        auto* editor = new KeySequenceEdit(m_shortcutsTable);
        editor->setText(m_shortcutsTable->item(row, 1)->text());
        m_shortcutsTable->setCellWidget(row, col, editor);
        editor->setFocus();
        connect(editor, &KeySequenceEdit::editingFinished, this, [this, row, editor]() {
            const QString cmdId = m_shortcutsTable->item(row, 0)->data(Qt::UserRole).toString();
            const QKeySequence seq = editor->sequence();
            QString conflict;
            if (m_keybinds->setBinding(cmdId, seq, &conflict)) {
                m_shortcutsTable->item(row, 1)->setText(seq.toString());
            } else {
                QMessageBox::warning(this, tr("Shortcut Conflict"),
                    tr("'%1' is already assigned. Choose another combination.").arg(seq.toString()));
            }
            m_shortcutsTable->removeCellWidget(row, 1);
        });
    });
}

void SettingsDialog::exportSettings()
{
    const QString file = QFileDialog::getSaveFileName(this, tr("Export Settings"), QString(),
                                                      QStringLiteral("Settings (*.json)"));
    if (file.isEmpty()) return;
    QString err;
    if (!m_settings->exportTo(file, &err))
        QMessageBox::warning(this, tr("Export"), err);
}

void SettingsDialog::importSettings()
{
    const QString file = QFileDialog::getOpenFileName(this, tr("Import Settings"), QString(),
                                                      QStringLiteral("Settings (*.json)"));
    if (file.isEmpty()) return;
    QString err;
    if (!m_settings->importFrom(file, &err)) {
        QMessageBox::warning(this, tr("Import"), err);
        return;
    }
    populateKeyboardTable();
    onPageChanged(m_categories->currentRow());
    QMessageBox::information(this, tr("Import"), tr("Settings imported. Some changes apply immediately."));
}

void SettingsDialog::resetSettings()
{
    if (QMessageBox::question(this, tr("Reset Settings"),
        tr("Reset ALL user settings to defaults?"), QMessageBox::Yes | QMessageBox::Cancel) != QMessageBox::Yes)
        return;
    m_settings->resetAll();
    onPageChanged(m_categories->currentRow());
}

void SettingsDialog::applyPendingEdits() {}

}  // namespace cf
