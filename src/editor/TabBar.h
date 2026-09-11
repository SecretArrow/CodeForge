#pragma once
// Custom tab bar: close buttons, modified/pin indicators, preview styling,
// manual drag & drop (reorder + move across editor groups), wheel switching.
#include <QTabBar>

class QToolButton;

namespace cf {

class TabBar : public QTabBar {
    Q_OBJECT
public:
    explicit TabBar(QWidget* parent = nullptr);

    // Refresh icon/text state for a tab.
    void updateTab(int index, const QString& title, bool dirty, bool pinned, bool preview, bool readOnly);

    static const QString dragMime() { return QStringLiteral("application/x-codeforge-tab"); }

signals:
    void closeRequested(int index);          // from close button or middle click
    void reorderRequested(int from, int to);
    void pinToggleRequested(int index);
    void duplicateRequested(int index);
    void externalDropRequested(const QString& docId, int index);   // tab dragged from another group
    void newUntitledRequested();

protected:
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dragMoveEvent(QDragMoveEvent* e) override;
    void dropEvent(QDropEvent* e) override;

private:
    void attachButtons(int index);
    int indexOfDocId(const QString& docId) const;
    int m_dragIndex = -1;
    QPoint m_dragStartPos;
    bool m_dragActive = false;
    QString m_dragDocId;
};

}  // namespace cf
