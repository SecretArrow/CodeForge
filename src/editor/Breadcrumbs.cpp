#include "editor/Breadcrumbs.h"

#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>

#include "core/TextDocument.h"
#include "project/Workspace.h"
#include "syntax/SymbolScanner.h"

namespace cf {

Workspace* Breadcrumbs::s_workspace = nullptr;

Breadcrumbs::Breadcrumbs(QWidget* parent)
    : QWidget(parent), m_layout(new QHBoxLayout(this))
{
    setObjectName(QStringLiteral("codeforge_breadcrumbs"));
    m_layout->setContentsMargins(10, 3, 10, 3);
    m_layout->setSpacing(2);
    m_layout->addStretch(1);
}

void Breadcrumbs::setDocument(TextDocument* doc)
{
    m_doc = doc;
    rebuild();
}

void Breadcrumbs::updateCursorLine(int line)
{
    if (line != m_line) {
        m_line = line;
        rebuild();
    }
}

void Breadcrumbs::rebuild()
{
    // Clear labels (keep stretch at the end).
    while (m_layout->count() > 1) {
        QLayoutItem* it = m_layout->takeAt(0);
        if (it->widget()) it->widget()->deleteLater();
        delete it;
    }

    if (!m_doc) { update(); return; }

    auto* mw = qobject_cast<QWidget*>(parent());
    Q_UNUSED(mw);

    // Path segments (workspace-relative).
    QString rel;
    if (s_workspace && s_workspace->isOpen())
        rel = s_workspace->relativePath(m_doc->filePath());
    else
        rel = QFileInfo(m_doc->filePath()).fileName();
    const QStringList segments = rel.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (int i = 0; i < segments.size(); ++i) {
        auto* seg = new QLabel(segments.at(i), this);
        m_layout->insertWidget(m_layout->count() - 1, seg);
        if (i < segments.size() - 1) {
            auto* sep = new QLabel(QStringLiteral("›"), this);
            sep->setObjectName(QStringLiteral("breadcrumb_sep"));
            m_layout->insertWidget(m_layout->count() - 1, sep);
        }
    }

    // Nearest symbol at or above the cursor.
    const QString lang = m_doc->languageId();
    if (!lang.isEmpty()) {
        const QList<SymbolInfo> symbols = SymbolScanner::scan(lang, m_doc->document()->toPlainText());
        const SymbolInfo* nearest = nullptr;
        for (const SymbolInfo& s : symbols) {
            if (s.line <= m_line) nearest = &s;
            else break;
        }
        if (nearest) {
            auto* sep = new QLabel(QStringLiteral("›"), this);
            m_layout->insertWidget(m_layout->count() - 1, sep);
            auto* sym = new QLabel(QStringLiteral("%1 (%2)").arg(nearest->name, nearest->kind), this);
            m_layout->insertWidget(m_layout->count() - 1, sym);
        }
    }
    update();
}

}  // namespace cf
