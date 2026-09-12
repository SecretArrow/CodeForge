#include "editor/ImagePreview.h"

#include <QImageReader>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include "core/Common.h"

namespace cf {

namespace {
QStringList imageExtensions()
{
    return { QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("bmp"),
             QStringLiteral("gif"), QStringLiteral("svg"), QStringLiteral("webp"), QStringLiteral("ico") };
}
}  // namespace

bool isImageFile(const QString& path)
{
    return imageExtensions().contains(fileExtensionOf(path));
}

ImagePreview::ImagePreview(TextDocument* doc, QWidget* parent)
    : QWidget(parent), m_doc(doc)
{
    setObjectName(QStringLiteral("codeforge_imagepreview"));
    setFocusPolicy(Qt::NoFocus);
    load();
}

void ImagePreview::load()
{
    m_error.clear();
    m_valid = false;
    m_image = QImage();
    m_zoom = 1.0;
    m_fit = true;

    if (m_doc->isUntitled()) {
        m_error = tr("Untitled buffer has no image to show.");
        update();
        return;
    }

    QImageReader reader(m_doc->filePath());
    reader.setAutoTransform(true);
    const QImage img = reader.read();
    if (img.isNull()) {
        m_error = tr("Cannot load image: %1").arg(reader.errorString());
        update();
        return;
    }
    m_image = img;
    m_valid = true;
    update();
}

void ImagePreview::zoomAt(double factor)
{
    m_fit = false;
    m_zoom = qBound(0.05, m_zoom * factor, 24.0);
    update();
}

void ImagePreview::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), palette().window().color());

    if (!m_valid) {
        p.setPen(palette().color(QPalette::PlaceholderText));
        p.drawText(rect(), Qt::AlignCenter, m_error);
        return;
    }

    double scale = m_zoom;
    if (m_fit) {
        const double sx = width() / double(m_image.width());
        const double sy = height() / double(m_image.height());
        scale = qMin(1.0, qMin(sx, sy));
    }
    const QSize target = QSize(int(m_image.width() * scale), int(m_image.height() * scale));
    const QImage scaled = m_image.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    const QRect imgRect(QPoint((width() - scaled.width()) / 2, (height() - scaled.height()) / 2), scaled.size());

    // Checkerboard behind transparent images.
    if (m_image.hasAlphaChannel()) {
        const int cell = 10;
        for (int y = 0; y < imgRect.height(); y += cell)
            for (int x = 0; x < imgRect.width(); x += cell) {
                const bool odd = ((x / cell) + (y / cell)) % 2 == 0;
                p.fillRect(QRect(x + imgRect.left(), y + imgRect.top(), qMin(cell, imgRect.width() - x),
                                 qMin(cell, imgRect.height() - y)),
                           odd ? QColor(200, 200, 200, 90) : QColor(120, 120, 120, 60));
            }
    }
    p.drawImage(imgRect.topLeft(), scaled);

    // Info overlay.
    const QString info = QStringLiteral("%1 × %2   %3%")
                             .arg(m_image.width())
                             .arg(m_image.height())
                             .arg(QString::number(scale * 100, 'f', 0));
    p.setPen(palette().color(QPalette::ToolTipText));
    p.drawText(QRect(8, 6, width() - 16, 18), Qt::AlignLeft, info);
}

void ImagePreview::wheelEvent(QWheelEvent* e)
{
    if (!m_valid) return;
    if (e->modifiers() & Qt::ControlModifier) {
        zoomAt(e->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15);
        e->accept();
    } else {
        QWidget::wheelEvent(e);
    }
}

void ImagePreview::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    update();
}

void ImagePreview::mouseDoubleClickEvent(QMouseEvent* e)
{
    Q_UNUSED(e);
    m_fit = true;
    m_zoom = 1.0;
    update();
}

}  // namespace cf
