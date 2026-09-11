#include "editor/TabBar.h"

#include <QDrag>
#include <QFont>
#include <QMouseEvent>
#include <QMimeData>
#include <QPainter>
#include <QToolButton>
#include <QWheelEvent>

#include "ui/Icons.h"

namespace cf {

TabBar::TabBar(QWidget* parent) : QTabBar(parent)
{
    setTabsClosable(false);          // custom close buttons
    setMovable(false);               // manual drag handling
    setExpanding(false);
    setElideMode(Qt::ElideRight);
    setUsesScrollButtons(true);
    setDrawBase(false);
    setAcceptDrops(true);
    setSelectionBehaviorOnRemove(QTabBar::SelectPreviousTab);
}

void TabBar::attachButtons(int index)
{
    auto* close = new QToolButton(this);
    close->setIcon(Icons::icon(Icons::Name::Close));
    close->setFixedSize(15, 15);
    close->setAutoRaise(true);
    close->setToolTip(tr("Close (Ctrl+W)"));
    setTabButton(index, QTabBar::RightSide, close);

    connect(close, &QToolButton::clicked, this, [this, close]() {
        for (int i = 0; i < count(); ++i)
            if (tabButton(i, QTabBar::RightSide) == close) { emit closeRequested(i); break; }
    });
}

void TabBar::updateTab(int index, const QString& title, bool dirty, bool pinned, bool preview, bool readOnly)
{
    if (index < 0 || index >= count()) return;
    setTabText(index, title);
    setTabToolTip(index, title);

    // Left side indicator: pin > modified dot > nothing.
    QWidget* old = tabButton(index, QTabBar::LeftSide);
    if (old) { old->deleteLater(); setTabButton(index, QTabBar::LeftSide, nullptr); }
    if (pinned || dirty) {
        auto* ind = new QToolButton(this);
        ind->setIcon(Icons::icon(pinned ? Icons::Name::Pin : Icons::Name::DotModified));
        ind->setFixedSize(14, 14);
        ind->setAutoRaise(true);
        ind->setEnabled(false);
        setTabButton(index, QTabBar::LeftSide, ind);
    }

    attachButtons(index);

    // Preview state is reflected by an italic-ish dimmed title color.
    setTabTextColor(index, preview ? palette().color(QPalette::Disabled, QPalette::Text)
                                   : palette().color(QPalette::WindowText));
}

void TabBar::mousePressEvent(QMouseEvent* e)
{
    const int idx = tabAt(e->pos());
    if (e->button() == Qt::MiddleButton && idx >= 0) {
        emit closeRequested(idx);
        return;
    }
    if (e->button() == Qt::LeftButton && idx >= 0) {
        m_dragIndex = idx;
        m_dragStartPos = e->pos();
        m_dragActive = false;
    }
    QTabBar::mousePressEvent(e);
}

void TabBar::mouseMoveEvent(QMouseEvent* e)
{
    if (m_dragIndex >= 0 && (e->buttons() & Qt::LeftButton)) {
        if (!m_dragActive && (e->pos() - m_dragStartPos).manhattanLength() > 12) {
            m_dragActive = true;
            const QString docId = tabData(m_dragIndex).toString();
            m_dragDocId = docId;
            auto* mime = new QMimeData();
            mime->setData(TabBar::dragMime(), docId.toUtf8());
            auto* drag = new QDrag(this);
            drag->setMimeData(mime);
            QPixmap pm(120, 22);
            pm.fill(palette().color(QPalette::Button));
            drag->setPixmap(pm);
            drag->exec(Qt::MoveAction);
            m_dragActive = false;
            m_dragIndex = -1;
            return;
        }
    }
    QTabBar::mouseMoveEvent(e);
}

void TabBar::mouseReleaseEvent(QMouseEvent* e)
{
    m_dragIndex = -1;
    m_dragActive = false;
    QTabBar::mouseReleaseEvent(e);
}

void TabBar::mouseDoubleClickEvent(QMouseEvent* e)
{
    const int idx = tabAt(e->pos());
    if (idx >= 0) emit pinToggleRequested(idx);
    else emit newUntitledRequested();
}

void TabBar::wheelEvent(QWheelEvent* e)
{
    const int delta = e->angleDelta().y();
    if (delta != 0 && count() > 1) {
        setCurrentIndex((currentIndex() + (delta > 0 ? -1 : 1) + count()) % count());
        e->accept();
        return;
    }
    QTabBar::wheelEvent(e);
}

void TabBar::dragEnterEvent(QDragEnterEvent* e)
{
    if (e->mimeData()->hasFormat(TabBar::dragMime())) e->acceptProposedAction();
}

void TabBar::dragMoveEvent(QDragMoveEvent* e)
{
    if (e->mimeData()->hasFormat(TabBar::dragMime())) e->acceptProposedAction();
}

void TabBar::dropEvent(QDropEvent* e)
{
    if (!e->mimeData()->hasFormat(TabBar::dragMime())) return;
    const QString docId = QString::fromUtf8(e->mimeData()->data(TabBar::dragMime()));
    const int sourceIdx = indexOfDocId(docId);
    int target = tabAt(e->pos());
    if (target < 0) target = count() - 1;

    if (sourceIdx >= 0 && sourceIdx != target) {
        if (target > sourceIdx) --target;
        emit reorderRequested(sourceIdx, target);
    } else if (sourceIdx < 0 && !docId.isEmpty()) {
        // Cross-group move: notify with insertion index.
        emit externalDropRequested(docId, target);
    }
    e->acceptProposedAction();
}

// helper: find tab index carrying this docId
int TabBar::indexOfDocId(const QString& docId) const
{
    for (int i = 0; i < count(); ++i)
        if (tabData(i).toString() == docId) return i;
    return -1;
}

}  // namespace cf
