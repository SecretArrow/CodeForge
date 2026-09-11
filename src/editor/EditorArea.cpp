#include "editor/EditorArea.h"

#include <QJsonArray>
#include <QSplitter>
#include <QVBoxLayout>
#include <QVariant>

#include <functional>

#include "core/DocumentManager.h"
#include "editor/EditorGroup.h"

namespace cf {

EditorArea::EditorArea(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_root = new QSplitter(Qt::Horizontal, this);
    m_root->setChildrenCollapsible(false);
    m_root->setObjectName(QStringLiteral("editor_root_splitter"));
    layout->addWidget(m_root);

    // Always start with one empty group so documents have somewhere to open.
    EditorGroup* initial = new EditorGroup(this);
    registerGroup(initial);
    m_root->addWidget(initial);
    m_active = initial;
}

void EditorArea::registerGroup(EditorGroup* g)
{
    m_groups.append(g);

    connect(g, &EditorGroup::focusActivated, this, [this](EditorGroup* fg) { setActiveGroup(fg); });
    connect(g, &EditorGroup::currentDocChanged, this, [this](TextDocument* doc) {
        if (sender() && static_cast<EditorGroup*>(sender()) == m_active)
            emit activeDocChanged(doc);
    });
    connect(g, &EditorGroup::becameEmpty, this, [this](EditorGroup* eg) {
        if (m_groups.size() > 1) {
            closeGroup(eg);
        } else {
            setActiveGroup(eg);
            emit activeDocChanged(nullptr);
        }
        emit openStateChanged(groupCount() > 0 && activeGroup()->tabCount() > 0);
    });
}

void EditorArea::unregisterGroup(EditorGroup* g)
{
    m_groups.removeAll(g);
}

void EditorArea::openDocument(TextDocument* doc, bool preview)
{
    if (doc && m_active) {
        m_active->openDocument(doc, preview);
        emit activeDocChanged(doc);
    }
}

void EditorArea::setActiveGroup(EditorGroup* g)
{
    if (!g || m_active == g) return;
    m_active = g;
    emit groupActivated(g);
    emit activeDocChanged(g->currentDocument());
}

EditorGroup* EditorArea::splitGroup(EditorGroup* g, Qt::Orientation orientation, bool moveCurrentTab)
{
    auto* newGroup = new EditorGroup(this);
    registerGroup(newGroup);

    QWidget* container = g;   // the widget inside its parent splitter
    auto* parent = qobject_cast<QSplitter*>(container->parentWidget());

    if (parent->orientation() == orientation) {
        const int idx = parent->indexOf(g);
        parent->insertWidget(idx + 1, newGroup);
    } else {
        auto* np = new QSplitter(orientation, this);
        np->setChildrenCollapsible(false);
        const int idx = parent->indexOf(g);
        parent->replaceWidget(idx, np);
        np->addWidget(g);
        np->addWidget(newGroup);
        parent->setSizes(parent->sizes());   // no-op refresh
    }

    // Equal-ish split.
    auto* pp = qobject_cast<QSplitter*>(newGroup->parentWidget());
    if (pp && pp->count() >= 2) {
        QList<int> sizes = pp->sizes();
        const int total = std::accumulate(sizes.begin(), sizes.end(), 0);
        if (total > 0) {
            const int half = total / sizes.size();
            sizes = QList<int>(sizes.size(), half);
            pp->setSizes(sizes);
        }
    }

    if (moveCurrentTab && g->currentDocument()) {
        TextDocument* doc = g->currentDocument();
        newGroup->openDocument(doc);
        g->closeTab(g->indexOfDoc(doc));
    }

    setActiveGroup(newGroup);
    emit openStateChanged(true);
    return newGroup;
}

void EditorArea::closeGroup(EditorGroup* g)
{
    if (!g || m_groups.size() <= 1) return;

    auto* parent = qobject_cast<QSplitter*>(g->parentWidget());
    if (!parent) return;

    // Focus a neighbor.
    EditorGroup* neighbor = nullptr;
    for (EditorGroup* cand : m_groups)
        if (cand != g) { neighbor = cand; break; }

    unregisterGroup(g);
    g->deleteLater();

    collapseIfPossible(parent);

    if (neighbor && m_groups.contains(neighbor)) {
        setActiveGroup(neighbor);
        emit activeDocChanged(neighbor->currentDocument());
    }
    emit openStateChanged(groupCount() > 0 && activeGroup() && activeGroup()->tabCount() > 0);
}

void EditorArea::collapseIfPossible(QSplitter* parent)
{
    // Remove empty splitters, merging single children upward.
    if (!parent || parent == m_root) return;
    if (parent->count() == 0) {
        auto* grand = qobject_cast<QSplitter*>(parent->parentWidget());
        parent->deleteLater();
        collapseIfPossible(grand);
        return;
    }
    if (parent->count() == 1) {
        QWidget* child = parent->widget(0);
        auto* grand = qobject_cast<QSplitter*>(parent->parentWidget());
        if (grand) {
            const int idx = grand->indexOf(parent);
            grand->replaceWidget(idx, child);
            parent->deleteLater();
            collapseIfPossible(grand);
        }
    }
}

QJsonObject EditorArea::saveState() const
{
    // Serialize the splitter tree recursively.
    std::function<QJsonObject(QWidget*)> ser = [&ser](QWidget* w) -> QJsonObject {
        QJsonObject o;
        if (auto* sp = qobject_cast<QSplitter*>(w)) {
            o.insert(QStringLiteral("type"), QStringLiteral("split"));
            o.insert(QStringLiteral("orientation"), sp->orientation() == Qt::Horizontal ? QStringLiteral("h") : QStringLiteral("v"));
            o.insert(QStringLiteral("sizes"), QJsonArray::fromVariantList(QVariantList()
                << QList<QVariant>(sp->sizes().begin(), sp->sizes().end())));
            QJsonArray children;
            for (int i = 0; i < sp->count(); ++i) children.append(ser(sp->widget(i)));
            o.insert(QStringLiteral("children"), children);
            return o;
        }
        if (auto* grp = qobject_cast<EditorGroup*>(w)) {
            o.insert(QStringLiteral("type"), QStringLiteral("group"));
            o.insert(QStringLiteral("tabs"), grp->saveState());
            return o;
        }
        return o;
    };

    QJsonObject state;
    state.insert(QStringLiteral("layout"), ser(m_root));
    if (m_active) state.insert(QStringLiteral("activeIndex"), m_groups.indexOf(m_active));
    return state;
}

bool EditorArea::restoreState(const QJsonObject& state)
{
    if (state.isEmpty()) return false;

    DocumentManager& dm = DocumentManager::instance();
    EditorGroup* lastGroup = nullptr;
    EditorGroup* activeGroupFound = nullptr;
    const int activeIndex = state.value(QStringLiteral("activeIndex")).toInt();

    std::function<bool(const QJsonObject&, QSplitter*)> build = [&](const QJsonObject& node, QSplitter* parent) -> bool {
        const QString type = node.value(QStringLiteral("type")).toString();
        if (type == QLatin1String("split")) {
            auto* sp = new QSplitter(node.value(QStringLiteral("orientation")).toString() == QLatin1String("h")
                                         ? Qt::Horizontal : Qt::Vertical, this);
            sp->setChildrenCollapsible(false);
            parent->addWidget(sp);
            bool any = false;
            for (const QJsonValue& v : node.value(QStringLiteral("children")).toArray())
                any = build(v.toObject(), sp) || any;
            // Only keep splitters that actually received groups.
            if (sp->count() == 0) { sp->deleteLater(); return any; }
            return any;
        }
        if (type == QLatin1String("group")) {
            auto* grp = new EditorGroup(this);
            registerGroup(grp);
            parent->addWidget(grp);
            lastGroup = grp;
            for (const QJsonValue& v : node.value(QStringLiteral("tabs")).toArray()) {
                const QJsonObject tab = v.toObject();
                const QString docId = tab.value(QStringLiteral("docId")).toString();
                TextDocument* doc = dm.documentById(docId);
                if (!doc && !docId.startsWith(QLatin1String("untitled:"))) {
                    QString err;
                    doc = dm.openDocument(docId, false, &err);
                }
                if (!doc && docId.startsWith(QLatin1String("untitled:"))) {
                    // Restore untitled buffers (content from encrypted recovery).
                    doc = dm.createUntitled();
                    dm.restoreRecoveryInto(doc);
                }
                if (doc) grp->openDocument(doc, false);
            }
            if (grp->tabCount() == 0 && lastGroup == grp) {
                // leave empty group open; welcome handles emptiness
            }
            if (m_groups.indexOf(grp) == activeIndex) activeGroupFound = grp;
            return true;
        }
        return false;
    };

    const QJsonObject layout = state.value(QStringLiteral("layout")).toObject();
    if (layout.isEmpty()) return false;
    build(layout, m_root);

    if (m_groups.isEmpty()) return false;
    setActiveGroup(activeGroupFound ? activeGroupFound : m_groups.first());
    emit openStateChanged(true);
    return true;
}

}  // namespace cf
