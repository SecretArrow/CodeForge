#pragma once
// Basic minimap: renders a compressed line-structure preview of the document
// with a viewport indicator; click/drag navigates.
#include <QElapsedTimer>
#include <QImage>
#include <QWidget>

namespace cf {

class CodeEditor;

class Minimap : public QWidget {
    Q_OBJECT
public:
    explicit Minimap(CodeEditor* editor);

    void scheduleRebuild();
    void syncViewport();
    void refreshFont(const QFont& f);

protected:
    void paintEvent(QPaintEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;

private:
    void rebuild();
    int docToMap(int blockNumber) const;
    int mapToDoc(int mapY) const;

    CodeEditor* m_editor;
    QImage m_cache;
    QFont m_font;
    QTimer* m_rebuildTimer;
    bool m_dragging = false;
    double m_samples = 0;      // doc blocks per cache row
};

}  // namespace cf
