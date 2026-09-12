#include "editor/Minimap.h"

#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QTextBlock>
#include <QTimer>

#include "editor/CodeEditor.h"

namespace cf {

static constexpr int kRowHeight = 3;
static constexpr int kMaxCacheHeight = 2400;

Minimap::Minimap(CodeEditor* editor)
    : QWidget(editor), m_editor(editor)
{
    setFixedWidth(84);
    setObjectName(QStringLiteral("codeforge_minimap"));

    m_rebuildTimer = new QTimer(this);
    m_rebuildTimer->setSingleShot(true);
    m_rebuildTimer->setInterval(300);
    connect(m_rebuildTimer, &QTimer::timeout, this, &Minimap::rebuild);
}

void Minimap::refreshFont(const QFont& f)
{
    m_font = f;
    m_font.setPointSizeF(2.2);
    scheduleRebuild();
}

void Minimap::scheduleRebuild()
{
    m_rebuildTimer->start();
}

void Minimap::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    scheduleRebuild();
}

void Minimap::rebuild()
{
    const QTextDocument* doc = m_editor->document();
    const int blockCount = doc->blockCount();
    if (blockCount == 0) { m_cache = QImage(); update(); return; }

    // Sample lines into a bounded cache so huge documents still render.
    const int targetRows = qMin(blockCount, qMax(200, height() * 2 / kRowHeight * 4));
    m_samples = double(blockCount) / double(targetRows);

    const int w = width() - 6;
    const int h = qMin(kMaxCacheHeight, targetRows * kRowHeight);
    m_cache = QImage(qMax(1, w), qMax(1, h), QImage::Format_ARGB32_Premultiplied);
    m_cache.fill(Qt::transparent);

    QPainter p(&m_cache);
    p.setRenderHint(QPainter::Antialiasing, false);
    const QColor lineColor = m_editor->palette().color(QPalette::Text);
    p.setPen(Qt::NoPen);

    QTextBlock b = doc->firstBlock();
    int blockIdx = 0;
    const QColor barColor = QColor(lineColor.red(), lineColor.green(), lineColor.blue(), 110);
    while (b.isValid()) {
        const int row = docToMap(b.blockNumber());
        if (row >= 0 && row < m_cache.height() / kRowHeight) {
            const QString text = b.text();
            int indent = 0;
            while (indent < text.size() && (text.at(indent) == u' ' || text.at(indent) == u'\t')) ++indent;
            const int indentPx = qMin(w / 2, indent * 2);
            const int lenPx = qMin(w - indentPx, text.trimmed().size() * 2);
            if (lenPx > 0) {
                p.setPen(Qt::NoPen);
                p.setBrush(barColor);
                p.drawRect(3 + indentPx, row * kRowHeight, lenPx, kRowHeight - 1);
            }
        }
        b = b.next();
        ++blockIdx;
    }
    update();
}

int Minimap::docToMap(int blockNumber) const
{
    return int(blockNumber / qMax(1e-6, m_samples));
}

int Minimap::mapToDoc(int mapY) const
{
    const int row = qMax(0, mapY / kRowHeight);
    return int(row * m_samples);
}

void Minimap::paintEvent(QPaintEvent* e)
{
    Q_UNUSED(e);
    QPainter p(this);
    p.fillRect(rect(), QColor(0, 0, 0, 40));

    if (!m_cache.isNull())
        p.drawImage(rect().adjusted(0, 0, -1, -1), m_cache);

    // Viewport indicator.
    QScrollBar* sb = m_editor->verticalScrollBar();
    const int total = qMax(1, m_editor->document()->blockCount());
    const int visible = qMax(1, m_editor->viewport()->height() / m_editor->fontMetrics().height());
    const int firstBlock = m_editor->firstVisibleBlock().blockNumber();

    const double scale = double(height()) / qMax(1, int(total / qMax(1e-6, m_samples) * kRowHeight + 1));
    Q_UNUSED(scale);
    const int mapTotal = int(total / qMax(1e-6, m_samples)) + 1;
    const int y1 = docToMap(firstBlock) * kRowHeight;
    const int y2 = qMin(height(), y1 + qMax(20, visible * kRowHeight * int(qMax(1.0, m_samples) / 4)));
    if (mapTotal > 0) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 28));
        p.drawRect(0, y1, width(), qMax(14, y2 - y1));
        p.setBrush(QColor(255, 255, 255, 50));
        p.drawRect(width() - 2, y1, 2, qMax(14, y2 - y1));
    }
    Q_UNUSED(sb);
}

void Minimap::syncViewport()
{
    update();
}

void Minimap::mousePressEvent(QMouseEvent* e)
{
    m_dragging = true;
    const int targetBlock = mapToDoc(int(e->position().y()));
    QTextCursor c = m_editor->textCursor();
    QTextBlock b = m_editor->document()->findBlockByNumber(targetBlock);
    if (b.isValid()) {
        c.setPosition(b.position());
        m_editor->setTextCursor(c);
        m_editor->centerCursor();
    }
}

void Minimap::mouseMoveEvent(QMouseEvent* e)
{
    if (!m_dragging) return;
    const int targetBlock = mapToDoc(int(e->position().y()));
    QScrollBar* sb = m_editor->verticalScrollBar();
    const int lineH = m_editor->fontMetrics().height();
    sb->setValue(targetBlock * lineH - m_editor->viewport()->height() / 2);
}

void Minimap::mouseReleaseEvent(QMouseEvent* e)
{
    Q_UNUSED(e);
    m_dragging = false;
}

}  // namespace cf
