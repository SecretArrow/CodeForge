#include "ui/Icons.h"

#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPixmap>

namespace cf {

namespace {

QHash<QString, QIcon>& cache()
{
    static QHash<QString, QIcon> c;
    return c;
}

QString cacheKey(Icons::Name name, const QColor& color)
{
    return QStringLiteral("%1|%2").arg(int(name)).arg(color.name());
}

QPainterPath chevronPath(qreal x, qreal y, qreal w, qreal h, bool right)
{
    QPainterPath p;
    if (right) {
        p.moveTo(x, y);
        p.lineTo(x + w * 0.5, y + h * 0.5);
        p.lineTo(x, y + h);
    } else {
        p.moveTo(x, y + h * 0.35);
        p.lineTo(x + w * 0.5, y + h * 0.85);
        p.lineTo(x + w, y + h * 0.35);
    }
    return p;
}

}  // namespace

QIcon Icons::icon(Name name, const QColor& color)
{
    const QString key = cacheKey(name, color);
    const auto it = cache().constFind(key);
    if (it != cache().constEnd()) return it.value();

    QPixmap pm(48, 48);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    paint(name, p, color, 48);
    p.end();

    QIcon ic(pm);
    cache().insert(key, ic);
    return ic;
}

QIcon Icons::icon(Name name)
{
    QColor c = qApp ? qApp->palette().color(QPalette::WindowText) : QColor(0xcc, 0xcc, 0xcc);
    return icon(name, c);
}

void Icons::paint(Name name, QPainter& p, const QColor& color, int size)
{
    QPen pen(color, size / 16.0);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    const qreal m = size * 0.15;                       // margin
    const qreal w = size - 2 * m;                      // drawing width
    QPen noPen; noPen.setStyle(Qt::NoPen);

    switch (name) {
    case Name::File:
        p.drawRoundedRect(QRectF(m, m + w*0.05, w*0.65, w*0.9), 1, 1);
        p.drawLine(QPointF(m + w*0.15, m + w*0.3), QPointF(m + w*0.5, m + w*0.3));
        p.drawLine(QPointF(m + w*0.15, m + w*0.5), QPointF(m + w*0.5, m + w*0.5));
        p.drawLine(QPointF(m + w*0.15, m + w*0.7), QPointF(m + w*0.4, m + w*0.7));
        break;
    case Name::Folder:
        p.drawRoundedRect(QRectF(m, m + w*0.15, w, w*0.7), 2, 2);
        p.drawRoundedRect(QRectF(m, m + w*0.08, w*0.5, w*0.2), 1, 1);
        break;
    case Name::FolderOpen:
        p.drawRoundedRect(QRectF(m, m + w*0.15, w, w*0.7), 2, 2);
        p.drawLine(QPointF(m, m + w*0.3), QPointF(m + w*0.9, m + w*0.3));
        p.drawLine(QPointF(m + w*0.55, m + w*0.3), QPointF(m + w*0.95, m + w*0.85));
        break;
    case Name::Search:
        p.drawEllipse(QRectF(m, m, w*0.62, w*0.62));
        p.drawLine(QPointF(m + w*0.55, m + w*0.55), QPointF(m + w, m + w));
        break;
    case Name::GitBranch: {
        p.drawEllipse(QRectF(m, m + w*0.7, w*0.22, w*0.22));
        p.drawEllipse(QRectF(m + w*0.78, m + w*0.7, w*0.22, w*0.22));
        p.drawEllipse(QRectF(m + w*0.78, m, w*0.22, w*0.22));
        p.drawLine(QPointF(m + w*0.11, m + w*0.7), QPointF(m + w*0.11, m + w*0.35));
        p.drawLine(QPointF(m + w*0.89, m + w*0.7), QPointF(m + w*0.89, m + w*0.22));
        p.drawLine(QPointF(m + w*0.11, m + w*0.35), QPointF(m + w*0.89, m + w*0.22));
        break;
    }
    case Name::Play: {
        QPainterPath tri;
        tri.moveTo(m + w*0.15, m + w*0.1);
        tri.lineTo(m + w*0.9, m + w*0.5);
        tri.lineTo(m + w*0.15, m + w*0.9);
        tri.closeSubpath();
        p.setPen(noPen);
        p.setBrush(color);
        p.drawPath(tri);
        break;
    }
    case Name::Puzzle:
        p.drawRoundedRect(QRectF(m + w*0.1, m + w*0.2, w*0.8, w*0.65), 2, 2);
        p.drawEllipse(QRectF(m + w*0.35, m + w*0.02, w*0.3, w*0.3));
        p.drawEllipse(QRectF(m + w*0.35, m + w*0.72, w*0.3, w*0.3));
        break;
    case Name::Gear: {
        p.drawEllipse(QRectF(m + w*0.3, m + w*0.3, w*0.4, w*0.4));
        for (int i = 0; i < 8; ++i) {
            const qreal a = i * M_PI / 4.0;
            p.drawLine(QPointF(m + w*0.5 + w*0.34*cos(a), m + w*0.5 + w*0.34*sin(a)),
                       QPointF(m + w*0.5 + w*0.48*cos(a), m + w*0.5 + w*0.48*sin(a)));
        }
        break;
    }
    case Name::Terminal:
        p.drawRoundedRect(QRectF(m, m + w*0.1, w, w*0.8), 2, 2);
        p.drawLine(QPointF(m + w*0.18, m + w*0.4), QPointF(m + w*0.35, m + w*0.55));
        p.drawLine(QPointF(m + w*0.35, m + w*0.55), QPointF(m + w*0.18, m + w*0.7));
        p.drawLine(QPointF(m + w*0.45, m + w*0.7), QPointF(m + w*0.75, m + w*0.7));
        break;
    case Name::Close:
        p.drawLine(QPointF(m + w*0.2, m + w*0.2), QPointF(m + w*0.8, m + w*0.8));
        p.drawLine(QPointF(m + w*0.8, m + w*0.2), QPointF(m + w*0.2, m + w*0.8));
        break;
    case Name::DotModified: {
        p.setPen(noPen);
        p.setBrush(color);
        p.drawEllipse(QRectF(size*0.35, size*0.35, size*0.3, size*0.3));
        break;
    }
    case Name::Pin: {
        p.drawLine(QPointF(m + w*0.5, m + w*0.45), QPointF(m + w*0.5, m + w*0.9));
        p.drawEllipse(QRectF(m + w*0.28, m + w*0.1, w*0.44, w*0.44));
        break;
    }
    case Name::SplitRight:
        p.drawRect(QRectF(m, m + w*0.1, w, w*0.8));
        p.drawLine(QPointF(m + w*0.5, m + w*0.1), QPointF(m + w*0.5, m + w*0.9));
        break;
    case Name::Plus:
        p.drawLine(QPointF(m + w*0.5, m + w*0.15), QPointF(m + w*0.5, m + w*0.85));
        p.drawLine(QPointF(m + w*0.15, m + w*0.5), QPointF(m + w*0.85, m + w*0.5));
        break;
    case Name::Refresh: {
        p.drawArc(QRectF(m + w*0.1, m + w*0.1, w*0.8, w*0.8), 45 * 16, 270 * 16);
        p.drawLine(QPointF(m + w*0.55, m + w*0.05), QPointF(m + w*0.85, m + w*0.25));
        break;
    }
    case Name::CollapseAll:
        p.drawLine(QPointF(m + w*0.2, m + w*0.4), QPointF(m + w*0.5, m + w*0.65));
        p.drawLine(QPointF(m + w*0.5, m + w*0.65), QPointF(m + w*0.8, m + w*0.4));
        p.drawLine(QPointF(m + w*0.2, m + w*0.7), QPointF(m + w*0.5, m + w*0.95));
        p.drawLine(QPointF(m + w*0.5, m + w*0.95), QPointF(m + w*0.8, m + w*0.7));
        break;
    case Name::Warning: {
        QPainterPath tri;
        tri.moveTo(m + w*0.5, m + w*0.08);
        tri.lineTo(m + w*0.95, m + w*0.88);
        tri.lineTo(m + w*0.05, m + w*0.88);
        tri.closeSubpath();
        p.drawPath(tri);
        p.drawLine(QPointF(m + w*0.5, m + w*0.42), QPointF(m + w*0.5, m + w*0.62));
        break;
    }
    case Name::Error:
        p.drawEllipse(QRectF(m + w*0.08, m + w*0.08, w*0.84, w*0.84));
        p.drawLine(QPointF(m + w*0.32, m + w*0.32), QPointF(m + w*0.68, m + w*0.68));
        p.drawLine(QPointF(m + w*0.68, m + w*0.32), QPointF(m + w*0.32, m + w*0.68));
        break;
    case Name::Info:
        p.drawEllipse(QRectF(m + w*0.08, m + w*0.08, w*0.84, w*0.84));
        p.drawLine(QPointF(m + w*0.5, m + w*0.42), QPointF(m + w*0.5, m + w*0.7));
        p.drawPoint(QPointF(m + w*0.5, m + w*0.3));
        break;
    case Name::Save:
        p.drawRoundedRect(QRectF(m, m + w*0.1, w, w*0.8), 2, 2);
        p.drawRect(QRectF(m + w*0.25, m + w*0.1, w*0.5, w*0.3));
        p.drawRect(QRectF(m + w*0.25, m + w*0.55, w*0.5, w*0.35));
        break;
    case Name::Book:
        p.drawRect(QRectF(m, m + w*0.1, w*0.4, w*0.8));
        p.drawRect(QRectF(m + w*0.6, m + w*0.1, w*0.4, w*0.8));
        p.drawLine(QPointF(m + w*0.4, m + w*0.1), QPointF(m + w*0.4, m + w*0.9));
        break;
    case Name::Lock: {
        p.drawRoundedRect(QRectF(m + w*0.15, m + w*0.45, w*0.7, w*0.5), 2, 2);
        p.drawArc(QRectF(m + w*0.28, m + w*0.05, w*0.44, w*0.6), 0, 180 * 16);
        break;
    }
    case Name::NewFile:
        Icons::paint(Name::File, p, color, size);
        p.setPen(QPen(color, size / 14.0));
        p.drawLine(QPointF(m + w*0.75, m + w*0.6), QPointF(m + w*0.75, m + w*0.95));
        p.drawLine(QPointF(m + w*0.58, m + w*0.78), QPointF(m + w*0.95, m + w*0.78));
        break;
    case Name::NewFolder:
        Icons::paint(Name::Folder, p, color, size);
        p.setPen(QPen(color, size / 14.0));
        p.drawLine(QPointF(m + w*0.75, m + w*0.5), QPointF(m + w*0.75, m + w*0.85));
        p.drawLine(QPointF(m + w*0.58, m + w*0.68), QPointF(m + w*0.95, m + w*0.68));
        break;
    case Name::Filter:
        p.drawLine(QPointF(m, m + w*0.2), QPointF(m + w, m + w*0.2));
        p.drawLine(QPointF(m + w*0.2, m + w*0.5), QPointF(m + w*0.8, m + w*0.5));
        p.drawLine(QPointF(m + w*0.4, m + w*0.8), QPointF(m + w*0.6, m + w*0.8));
        break;
    case Name::Outline:
        p.drawLine(QPointF(m, m + w*0.1), QPointF(m + w*0.3, m + w*0.1));
        p.drawLine(QPointF(m + w*0.15, m + w*0.1), QPointF(m + w*0.15, m + w*0.9));
        p.drawLine(QPointF(m + w*0.4, m + w*0.35), QPointF(m + w, m + w*0.35));
        p.drawLine(QPointF(m + w*0.4, m + w*0.6), QPointF(m + w*0.9, m + w*0.6));
        break;
    case Name::ChevronRight:
        p.drawPath(chevronPath(m + w*0.3, m + w*0.2, w*0.45, w*0.6, true));
        break;
    case Name::ChevronDown:
        p.drawPath(chevronPath(m + w*0.2, m + w*0.35, w*0.6, w*0.45, false));
        break;
    case Name::Copy:
        p.drawRoundedRect(QRectF(m + w*0.25, m + w*0.25, w*0.6, w*0.65), 1, 1);
        p.drawRoundedRect(QRectF(m + w*0.1, m + w*0.1, w*0.6, w*0.65), 1, 1);
        break;
    case Name::Trash:
        p.drawRoundedRect(QRectF(m + w*0.2, m + w*0.25, w*0.6, w*0.65), 1, 1);
        p.drawLine(QPointF(m + w*0.1, m + w*0.25), QPointF(m + w*0.9, m + w*0.25));
        p.drawLine(QPointF(m + w*0.38, m + w*0.25), QPointF(m + w*0.38, m + w*0.12));
        p.drawLine(QPointF(m + w*0.62, m + w*0.25), QPointF(m + w*0.62, m + w*0.12));
        break;
    case Name::Checklist:
        p.drawRoundedRect(QRectF(m + w*0.12, m + w*0.12, w*0.76, w*0.76), 1, 1);
        // checkboxes column
        p.drawLine(QPointF(m + w*0.24, m + w*0.32), QPointF(m + w*0.32, m + w*0.40));
        p.drawLine(QPointF(m + w*0.32, m + w*0.40), QPointF(m + w*0.44, m + w*0.22));
        p.drawLine(QPointF(m + w*0.24, m + w*0.62), QPointF(m + w*0.32, m + w*0.70));
        p.drawLine(QPointF(m + w*0.32, m + w*0.70), QPointF(m + w*0.44, m + w*0.52));
        // text lines
        p.drawLine(QPointF(m + w*0.54, m + w*0.32), QPointF(m + w*0.82, m + w*0.32));
        p.drawLine(QPointF(m + w*0.54, m + w*0.62), QPointF(m + w*0.82, m + w*0.62));
        break;
    }
}

}  // namespace cf
