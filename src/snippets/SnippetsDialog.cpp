#include "snippets/SnippetsDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

#include "snippets/SnippetStore.h"
#include "syntax/LanguageRegistry.h"

namespace cf {

SnippetsDialog::SnippetsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Snippets"));
    resize(760, 520);

    auto* layout = new QVBoxLayout(this);

    auto* topRow = new QHBoxLayout;
    m_list = new QListWidget(this);
    m_list->setMinimumWidth(220);
    topRow->addWidget(m_list, 1);

    auto* listButtons = new QVBoxLayout;
    auto* addBtn = new QPushButton(tr("Add"));
    auto* removeBtn = new QPushButton(tr("Remove"));
    listButtons->addWidget(addBtn);
    listButtons->addWidget(removeBtn);
    listButtons->addStretch(1);
    topRow->addLayout(listButtons);
    layout->addLayout(topRow);

    auto* form = new QFormLayout;
    m_trigger = new QLineEdit(this);
    m_description = new QLineEdit(this);
    m_languages = new QLineEdit(this);
    m_languages->setPlaceholderText(tr("comma separated: cpp, python, ... (empty = all)"));
    form->addRow(tr("Trigger"), m_trigger);
    form->addRow(tr("Description"), m_description);
    form->addRow(tr("Languages"), m_languages);
    layout->addLayout(form);

    layout->addWidget(new QLabel(tr("Body ($0 = caret position after expansion):"), this));
    m_body = new QPlainTextEdit(this);
    QFont mono(QStringLiteral("Consolas"));
    mono.setStyleHint(QFont::Monospace);
    m_body->setFont(mono);
    layout->addWidget(m_body, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Close, this);
    layout->addWidget(buttons);

    connect(m_list, &QListWidget::currentRowChanged, this, &SnippetsDialog::onSelectionChanged);
    connect(m_trigger, &QLineEdit::textEdited, this, &SnippetsDialog::onFieldChanged);
    connect(m_description, &QLineEdit::textEdited, this, &SnippetsDialog::onFieldChanged);
    connect(m_languages, &QLineEdit::textEdited, this, &SnippetsDialog::onFieldChanged);
    connect(m_body, &QPlainTextEdit::textChanged, this, &SnippetsDialog::onFieldChanged);
    connect(addBtn, &QPushButton::clicked, this, &SnippetsDialog::onAdd);
    connect(removeBtn, &QPushButton::clicked, this, &SnippetsDialog::onRemove);
    connect(buttons->button(QDialogButtonBox::Save), &QPushButton::clicked, this,
            &SnippetsDialog::onSave);
    connect(buttons->button(QDialogButtonBox::Close), &QPushButton::clicked, this, &QDialog::accept);

    reloadList(0);
}

void SnippetsDialog::reloadList(int selectRow)
{
    m_updating = true;
    m_list->clear();
    const QVector<Snippet> user = SnippetStore::instance().userSnippets();
    for (const Snippet& s : user) {
        m_list->addItem(QStringLiteral("%1  —  %2").arg(s.trigger, s.description));
    }
    m_updating = false;
    if (selectRow >= 0 && selectRow < m_list->count())
        m_list->setCurrentRow(selectRow);
    onSelectionChanged();
}

void SnippetsDialog::onSelectionChanged()
{
    const int row = m_list->currentRow();
    const QVector<Snippet> user = SnippetStore::instance().userSnippets();
    m_updating = true;
    if (row >= 0 && row < user.size()) {
        const Snippet& s = user[row];
        m_trigger->setText(s.trigger);
        m_description->setText(s.description);
        m_languages->setText(s.languages.join(QStringLiteral(", ")));
        m_body->setPlainText(s.body);
        m_trigger->setEnabled(true);
        m_description->setEnabled(true);
        m_languages->setEnabled(true);
        m_body->setEnabled(true);
    } else {
        m_trigger->clear();
        m_description->clear();
        m_languages->clear();
        m_body->clear();
        m_trigger->setEnabled(false);
        m_description->setEnabled(false);
        m_languages->setEnabled(false);
        m_body->setEnabled(false);
    }
    m_updating = false;
}

void SnippetsDialog::onFieldChanged()
{
    if (m_updating)
        return;
    const int row = m_list->currentRow();
    QVector<Snippet> user = SnippetStore::instance().userSnippets();
    if (row < 0 || row >= user.size())
        return;
    user[row].trigger = m_trigger->text();
    user[row].description = m_description->text();
    user[row].languages = m_languages->text().simplified()
                              .split(QStringLiteral(","), Qt::SkipEmptyParts);
    for (QString& l : user[row].languages)
        l = l.trimmed();
    user[row].body = m_body->toPlainText();
    SnippetStore::instance().setUserSnippets(user);
    const QString trig = m_trigger->text();
    m_list->item(row)->setText(QStringLiteral("%1  —  %2").arg(trig, m_description->text()));
}

void SnippetsDialog::onAdd()
{
    QVector<Snippet> user = SnippetStore::instance().userSnippets();
    Snippet s;
    s.trigger = QStringLiteral("newSnippet");
    s.body = QStringLiteral("$0");
    s.description = tr("new snippet");
    user.append(s);
    SnippetStore::instance().setUserSnippets(user);
    reloadList(user.size() - 1);
    m_trigger->setFocus();
    m_trigger->selectAll();
}

void SnippetsDialog::onRemove()
{
    const int row = m_list->currentRow();
    QVector<Snippet> user = SnippetStore::instance().userSnippets();
    if (row < 0 || row >= user.size())
        return;
    if (QMessageBox::question(this, tr("Snippets"),
                              tr("Remove snippet \"%1\"?").arg(user[row].trigger))
        != QMessageBox::Yes)
        return;
    user.removeAt(row);
    SnippetStore::instance().setUserSnippets(user);
    reloadList(qMin(row, user.size() - 1));
}

void SnippetsDialog::onSave()
{
    // fields already persist on change; Save gives explicit feedback
    QMessageBox::information(this, tr("Snippets"),
                             tr("Saved to %1").arg(SnippetStore::instance().userSnippetsPath()));
}

}  // namespace cf
