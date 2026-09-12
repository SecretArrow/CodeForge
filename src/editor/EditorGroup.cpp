#include "editor/EditorGroup.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QMenu>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "core/Common.h"
#include "core/DocumentManager.h"
#include "editor/CodeEditor.h"
#include "editor/EditorServices.h"
#include "editor/FindReplaceBar.h"
#include "editor/ImagePreview.h"
#include "editor/MarkdownPreview.h"
#include "editor/TabBar.h"
#include "themes/ThemeManager.h"

#include <QSplitter>

namespace cf {

bool isMarkdownFile(const QString& path)
{
    const QString ext = fileExtensionOf(path);
    return ext == QLatin1String("md") || ext == QLatin1String("markdown");
}

EditorGroup::EditorGroup(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_tabs = new TabBar(this);
    m_stack = new QStackedWidget(this);
    m_stack->setObjectName(QStringLiteral("editor_stack"));
    m_find = new FindReplaceBar(nullptr, this);
    m_find->hide();

    layout->addWidget(m_tabs);
    layout->addWidget(m_find);
    layout->addWidget(m_stack, 1);

    connect(m_tabs, &TabBar::closeRequested, this, [this](int i) { closeTab(i); });
    connect(m_tabs, &QTabBar::currentChanged, this, [this](int idx) {
        if (idx >= 0 && idx < m_docs.size()) activateTab(idx);
    });
    connect(m_tabs, &TabBar::reorderRequested, this, [this](int from, int to) {
        if (from >= 0 && from < m_docs.size() && to >= 0 && to <= m_docs.size()) moveTab(from, to);
    });
    connect(m_tabs, &TabBar::externalDropRequested, this, [this](const QString& docId, int index) {
        TextDocument* doc = DocumentManager::instance().documentById(docId);
        if (doc && !containsDoc(doc)) insertTabAt(doc, qBound(0, index, m_docs.size()));
    });
    connect(m_tabs, &TabBar::pinToggleRequested, this, &EditorGroup::togglePin);
    connect(m_tabs, &TabBar::newUntitledRequested, this, [this]() {
        openDocument(DocumentManager::instance().createUntitled());
    });

    installEventFilter(this);
    m_stack->installEventFilter(this);
}

bool EditorGroup::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::FocusIn)
        emit focusActivated(this);
    return QWidget::eventFilter(watched, event);
}

QWidget* EditorGroup::createView(TextDocument* doc)
{
    if (isImageFile(doc->filePath())) {
        auto* preview = new ImagePreview(doc, m_stack);
        return preview;
    }
    return createEditor(doc);
}

CodeEditor* EditorGroup::createEditor(TextDocument* doc)
{
    auto* editor = new CodeEditor(doc, m_stack);
    editor->applySettings();
    editor->applyEditorConfig();
    editor->applyTheme(ThemeManager::instance().currentTheme());
    connect(editor, &CodeEditor::focusGained, this, [this]() { emit focusActivated(this); });
    connect(editor, &CodeEditor::contextRequested, this, [this, editor](const QPoint& globalPos) {
        QMenu menu(editor);
        menu.addAction(tr("Undo"), editor, &QPlainTextEdit::undo)->setEnabled(editor->document()->isUndoAvailable());
        menu.addAction(tr("Redo"), editor, &QPlainTextEdit::redo)->setEnabled(editor->document()->isRedoAvailable());
        menu.addSeparator();
        menu.addAction(tr("Cut"), editor, &QPlainTextEdit::cut);
        menu.addAction(tr("Copy"), editor, &QPlainTextEdit::copy);
        menu.addAction(tr("Paste"), editor, &QPlainTextEdit::paste);
        menu.addSeparator();
        menu.addAction(tr("Select All"), editor, &QPlainTextEdit::selectAll);
        menu.addSeparator();
        QAction* find = menu.addAction(tr("Find"));
        connect(find, &QAction::triggered, this, [this]() { showFind(false); });
        QAction* replace = menu.addAction(tr("Replace"));
        connect(replace, &QAction::triggered, this, [this]() { showFind(true); });
        menu.exec(globalPos);
    });
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, editor,
            [editor](const Theme& t) { editor->applyTheme(t); });
    EditorServices::instance().notifyEditorCreated(editor);
    return editor;
}

void EditorGroup::openDocument(TextDocument* doc, bool preview)
{
    if (!doc) return;

    // Already open here -> just activate.
    const int existing = indexOfDoc(doc);
    if (existing >= 0) {
        activateTab(existing);
        return;
    }

    // Preview mode replaces the existing preview tab.
    if (preview && m_previewIndex >= 0 && m_previewIndex < m_docs.size()) {
        const int pi = m_previewIndex;
        QWidget* oldView = m_stack->widget(pi);
        m_docs[pi] = doc;
        m_pinned[pi] = false;
        if (oldView) {
            m_stack->removeWidget(oldView);
            oldView->deleteLater();
        }
        m_stack->insertWidget(pi, createView(doc));
        m_tabs->setTabData(pi, doc->docId());
        m_tabs->updateTab(pi, doc->displayName(), doc->isDirty(), false, preview, doc->isReadOnly());
        activateTab(pi);
        return;
    }

    const int index = m_docs.size();
    m_docs.append(doc);
    m_pinned.append(false);
    if (preview) m_previewIndex = index;
    m_stack->addWidget(createView(doc));
    m_tabs->addTab(doc->displayName());
    m_tabs->setTabData(index, doc->docId());
    m_tabs->updateTab(index, doc->displayName(), doc->isDirty(), false, preview, doc->isReadOnly());
    connectDocSignals(doc);
    activateTab(index);
}

// (editor widget removal is performed inline where needed)

void EditorGroup::connectDocSignals(TextDocument* doc)
{
    connect(doc, &TextDocument::dirtyChanged, this, [this, doc](bool) { refreshTabs(); });
    connect(doc, &TextDocument::pathChanged, this, [this, doc]() { refreshTabs(); });
    connect(doc, &TextDocument::dirtyChanged, this, [this, doc](bool dirty) {
        // Editing a preview tab promotes it to a normal tab.
        if (dirty) {
            const int idx = indexOfDoc(doc);
            if (idx >= 0) promotePreview(idx);
        }
    });
    connect(doc, &QObject::destroyed, this, [this, doc]() {
        // Safety: if a doc object dies unexpectedly, drop its tab.
        const int idx = indexOfDoc(doc);
        if (idx >= 0) {
            m_docs.removeAt(idx);
            m_pinned.remove(idx);
            QWidget* w = m_stack->widget(idx);
            m_stack->removeWidget(w);
            if (w) w->deleteLater();
            m_tabs->removeTab(idx);
            refreshTabs();
        }
    });
}

void EditorGroup::activateDocument(TextDocument* doc)
{
    const int idx = indexOfDoc(doc);
    if (idx >= 0) activateTab(idx);
}

void EditorGroup::activateTab(int index)
{
    if (index < 0 || index >= m_docs.size()) return;
    m_current = index;
    if (m_tabs->currentIndex() != index) m_tabs->blockSignals(true);
    m_tabs->setCurrentIndex(index);
    m_tabs->blockSignals(false);
    m_stack->setCurrentIndex(index);

    CodeEditor* ed = qobject_cast<CodeEditor*>(m_stack->widget(index));
    if (ed) {
        m_find->setEditor(ed);
        ed->setFocus();
        ed->updateExtraSelections();
    }
    promotePreview(index);
    emit currentDocChanged(m_docs.at(index));
}

void EditorGroup::promotePreview(int index)
{
    if (m_previewIndex == index) {
        m_previewIndex = -1;
        m_tabs->updateTab(index, m_docs.at(index)->displayName(), m_docs.at(index)->isDirty(),
                          m_pinned.at(index), false, m_docs.at(index)->isReadOnly());
    }
}

TextDocument* EditorGroup::currentDocument() const
{
    return (m_current >= 0 && m_current < m_docs.size()) ? m_docs.at(m_current) : nullptr;
}

CodeEditor* EditorGroup::currentEditor() const
{
    return qobject_cast<CodeEditor*>(m_stack->currentWidget());
}

CodeEditor* EditorGroup::editorForDoc(TextDocument* doc) const
{
    const int idx = indexOfDoc(doc);
    return idx >= 0 ? qobject_cast<CodeEditor*>(m_stack->widget(idx)) : nullptr;
}

bool EditorGroup::containsDoc(TextDocument* doc) const
{
    return indexOfDoc(doc) >= 0;
}

int EditorGroup::indexOfDoc(TextDocument* doc) const
{
    return m_docs.indexOf(doc);
}

bool EditorGroup::closeTab(int index)
{
    if (index < 0 || index >= m_docs.size()) return true;
    TextDocument* doc = m_docs.at(index);

    if (doc->isDirty() && !doc->isReadOnly()) {
        // Standard confirm flow via DocumentManager callbacks.
        DocumentManager& dm = DocumentManager::instance();
        if (dm.promptSaveBeforeClose) {
            const DocumentManager::SaveDecision d = dm.promptSaveBeforeClose(doc);
            if (d == DocumentManager::SaveDecision::Cancel) return false;
            if (d == DocumentManager::SaveDecision::Save) {
                if (doc->isUntitled()) {
                    // MainWindow runs Save-As via command; simplest path: open Save As flow.
                    emit statusMessage(QStringLiteral("save-untitled-requested"));
                    return false;
                }
                QString err;
                if (!dm.saveDocument(doc, &err)) {
                    if (dm.showError) dm.showError(err);
                    return false;
                }
            }
        }
    }

    const bool wasCurrent = (index == m_current);
    if (m_previewIndex == index) m_previewIndex = -1;
    else if (m_previewIndex > index) --m_previewIndex;

    m_docs.removeAt(index);
    m_pinned.removeAt(index);
    QWidget* editor = m_stack->widget(index);
    m_stack->removeWidget(editor);
    editor->deleteLater();
    m_tabs->removeTab(index);

    if (m_current > index) --m_current;
    if (m_current >= m_docs.size()) m_current = m_docs.size() - 1;

    if (m_docs.isEmpty()) {
        emit becameEmpty(this);
        emit currentDocChanged(nullptr);
    } else {
        refreshTabs();
        if (wasCurrent) activateTab(m_current);
    }
    return true;
}

void EditorGroup::closeOtherTabs(int index)
{
    for (int i = m_docs.size() - 1; i >= 0; --i)
        if (i != index && !m_pinned.at(i)) { if (!closeTab(i)) return; }
}

void EditorGroup::closeAllTabs()
{
    for (int i = m_docs.size() - 1; i >= 0; --i)
        if (!m_pinned.at(i)) { if (!closeTab(i)) return; }
}

void EditorGroup::closeSavedTabs()
{
    for (int i = m_docs.size() - 1; i >= 0; --i)
        if (!m_pinned.at(i) && !m_docs.at(i)->isDirty()) closeTab(i);
}

void EditorGroup::nextTab()
{
    if (!m_docs.isEmpty()) activateTab((m_current + 1) % m_docs.size());
}

void EditorGroup::previousTab()
{
    if (!m_docs.isEmpty()) activateTab((m_current - 1 + m_docs.size()) % m_docs.size());
}

void EditorGroup::togglePin(int index)
{
    if (index < 0 || index >= m_pinned.size()) return;
    m_pinned[index] = !m_pinned[index];
    refreshTabs();
}

void EditorGroup::moveTab(int from, int to)
{
    if (from < 0 || from >= m_docs.size() || to < 0 || to > m_docs.size()) return;
    m_docs.move(from, to);
    m_pinned.move(from, to);

    // Rebuild tab labels order in the TabBar.
    const bool blocked = m_tabs->blockSignals(true);
    QString data = m_tabs->tabData(from).toString();
    m_tabs->removeTab(from);
    m_tabs->insertTab(to, m_docs.at(to)->displayName());
    m_tabs->setTabData(to, data);
    m_tabs->blockSignals(blocked);

    if (m_current == from) m_current = to;
    else if (from < m_current && to >= m_current) --m_current;
    else if (from > m_current && to <= m_current) ++m_current;

    refreshTabs();
    activateTab(m_current);
}

void EditorGroup::insertTabAt(TextDocument* doc, int index)
{
    if (!doc) return;
    m_docs.insert(index, doc);
    m_pinned.insert(index, false);
    m_stack->insertWidget(index, createView(doc));
    m_tabs->insertTab(index, doc->displayName());
    m_tabs->setTabData(index, doc->docId());
    connectDocSignals(doc);
    activateTab(index);
}

void EditorGroup::showFind(bool replace)
{
    if (!currentEditor()) return;   // image viewer has no text to search
    if (replace) m_find->openReplace();
    else m_find->openFind();
}

void EditorGroup::toggleMarkdownPreview()
{
    if (m_markdownPreview) {
        // Tear down: restore the stack directly into the layout.
        layout()->removeWidget(m_previewSplitter);
        m_previewSplitter->hide();
        m_previewSplitter->deleteLater();
        m_previewSplitter = nullptr;
        m_markdownPreview = nullptr;
        static_cast<QVBoxLayout*>(layout())->addWidget(m_stack, 1);
        m_stack->show();
        return;
    }

    TextDocument* doc = currentDocument();
    if (!doc || !isMarkdownFile(doc->filePath())) {
        emit statusMessage(QStringLiteral("markdown-preview-unavailable"));
        return;
    }

    m_markdownPreview = new MarkdownPreview(this);
    m_previewSplitter = new QSplitter(Qt::Vertical, this);
    m_previewSplitter->setChildrenCollapsible(false);
    layout()->removeWidget(m_stack);
    m_previewSplitter->addWidget(m_stack);
    m_previewSplitter->addWidget(m_markdownPreview);
    m_previewSplitter->setStretchFactor(0, 3);
    m_previewSplitter->setStretchFactor(1, 2);
    m_previewSplitter->setSizes({ 600, 300 });
    static_cast<QVBoxLayout*>(layout())->addWidget(m_previewSplitter, 1);

    m_markdownPreview->setDocument(doc);
    connect(this, &EditorGroup::currentDocChanged, m_markdownPreview, [this](TextDocument* d) {
        if (m_markdownPreview)
            m_markdownPreview->setDocument(d && isMarkdownFile(d->filePath()) ? d : nullptr);
    });
}

void EditorGroup::refreshTabs()
{
    for (int i = 0; i < m_docs.size(); ++i) {
        TextDocument* doc = m_docs.at(i);
        m_tabs->updateTab(i, doc->displayName(), doc->isDirty(), m_pinned.at(i),
                          i == m_previewIndex, doc->isReadOnly());
    }
}

QJsonArray EditorGroup::saveState() const
{
    QJsonArray arr;
    for (int i = 0; i < m_docs.size(); ++i) {
        QJsonObject o;
        o.insert(QStringLiteral("docId"), m_docs.at(i)->docId());
        o.insert(QStringLiteral("pinned"), m_pinned.at(i));
        o.insert(QStringLiteral("active"), i == m_current);
        arr.append(o);
    }
    return arr;
}

}  // namespace cf
