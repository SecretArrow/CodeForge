#include "ui/OutlinePanel.h"

#include <QBoxLayout>
#include <QLabel>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "core/TextDocument.h"
#include "editor/CodeEditor.h"
#include "editor/EditorArea.h"
#include "editor/EditorGroup.h"
#include "syntax/SymbolScanner.h"

namespace cf {

OutlinePanel::OutlinePanel(EditorArea* editorArea, QWidget* parent)
    : QWidget(parent), m_editorArea(editorArea)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(6);

    auto* title = new QLabel(tr("Outline"), this);
    QFont f = title->font();
    f.setBold(true);
    title->setFont(f);
    layout->addWidget(title);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(false);
    layout->addWidget(m_tree, 1);

    connect(m_tree, &QTreeWidget::itemActivated, this, &OutlinePanel::onSymbolActivated);
    connect(m_tree, &QTreeWidget::itemClicked, this, &OutlinePanel::onSymbolActivated);
    clearOutline();
}

void OutlinePanel::clearOutline()
{
    m_tree->clear();
    m_tree->addTopLevelItem(new QTreeWidgetItem({ tr("Open a file to see its outline.") }));
}

void OutlinePanel::refreshForDocument(TextDocument* doc)
{
    if (!doc) { clearOutline(); return; }
    m_doc = doc;
    m_tree->clear();

    const QString lang = doc->languageId();
    if (lang.isEmpty()) {
        m_tree->addTopLevelItem(new QTreeWidgetItem({ tr("No language mode for this file.") }));
        return;
    }
    const QList<SymbolInfo> symbols = SymbolScanner::scan(lang, doc->document()->toPlainText());
    if (symbols.isEmpty()) {
        m_tree->addTopLevelItem(new QTreeWidgetItem({ tr("No symbols found.") }));
        return;
    }
    for (const SymbolInfo& s : symbols) {
        auto* item = new QTreeWidgetItem(m_tree);
        item->setText(0, QStringLiteral("%1  %2").arg(s.name, s.kind));
        item->setData(0, Qt::UserRole, s.line);
        item->setTextAlignment(0, Qt::AlignLeft);
    }
}

void OutlinePanel::onSymbolActivated(QTreeWidgetItem* item, int col)
{
    Q_UNUSED(col);
    if (!item || !m_doc || !m_editorArea) return;
    const bool hasData = item->data(0, Qt::UserRole).isValid();
    if (!hasData) return;
    CodeEditor* editor = m_editorArea->activeGroup() ? m_editorArea->activeGroup()->editorForDoc(m_doc) : nullptr;
    if (!editor) return;
    editor->gotoLine(item->data(0, Qt::UserRole).toInt());
}

}  // namespace cf
