#pragma once
// ImagePreview: image viewer shown as an editor tab for image files
// (png/jpg/bmp/gif/svg/webp). Wheel zoom, fit-to-window, real file loading.
#include <QWidget>

#include "core/TextDocument.h"

class QLabel;

namespace cf {

bool isImageFile(const QString& path);

class ImagePreview : public QWidget {
    Q_OBJECT
public:
    explicit ImagePreview(TextDocument* doc, QWidget* parent = nullptr);

    bool isValid() const { return m_valid; }

protected:
    void paintEvent(QPaintEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;

private:
    void load();
    void zoomAt(double factor);

    TextDocument* m_doc;
    QImage m_image;
    QString m_error;
    bool m_valid = false;
    double m_zoom = 1.0;      // 0 = fit-to-window
    bool m_fit = true;
};

}  // namespace cf
