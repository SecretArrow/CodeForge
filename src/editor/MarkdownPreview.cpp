#include "editor/MarkdownPreview.h"

#include <QPointer>

#include "markdown/MarkdownConverter.h"
#include "themes/Theme.h"
#include "themes/ThemeManager.h"

namespace cf {

MarkdownPreview::MarkdownPreview(QWidget* parent)
    : QTextBrowser(parent)
{
    setObjectName(QStringLiteral("codeforge_markdownpreview"));
    setOpenExternalLinks(true);
    setFrameStyle(QFrame::NoFrame);
    setReadOnly(true);

    m_debounce.setSingleShot(true);
    m_debounce.setInterval(250);
    connect(&m_debounce, &QTimer::timeout, this, &MarkdownPreview::render);

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this,
            [this](const Theme&) { render(); });
    render();
}

void MarkdownPreview::setDocument(TextDocument* doc)
{
    if (m_doc) {
        disconnect(m_doc->document(), &QTextDocument::contentsChange, this, nullptr);
        disconnect(m_doc->document(), &QTextDocument::destroyed, this, nullptr);
    }
    m_doc = doc;
    if (m_doc)
        connect(m_doc->document(), &QTextDocument::contentsChange, &m_debounce,
                qOverload<>(&QTimer::start));
    render();
}

void MarkdownPreview::render()
{
    const Theme t = ThemeManager::instance().currentTheme();
    const QColor bg = t.editorBackground();
    const QColor fg = t.editorForeground();
    const QColor accent = t.color(QStringLiteral("editor.link"));
    const QColor border = fg.darker(150);

    if (!m_doc) {
        QString css = QStringLiteral("body { color: %1; background: %2; font-family: sans-serif; }")
                          .arg(fg.name(), bg.name());
        setHtml(QStringLiteral("<html><head><style>%1</style></head><body><p><i>%2</i></p></body></html>")
                    .arg(css, tr("Open a Markdown file to see its preview here.")));
        return;
    }

    const QString body = markdown::toHtml(m_doc->document()->toPlainText());
    const QString css = QStringLiteral(
        R"(body { color: %1; background: %2; font-family: sans-serif; line-height: 1.5; padding: 12px; }
h1, h2, h3, h4, h5, h6 { color: %1; border-bottom: 1px solid %3; padding-bottom: 4px; }
a { color: %4; }
code { background: %5; padding: 1px 4px; border-radius: 3px; font-family: monospace; }
pre { background: %5; border: 1px solid %3; border-radius: 4px; padding: 8px; }
pre code { background: transparent; padding: 0; }
blockquote { border-left: 3px solid %4; margin-left: 0; padding-left: 10px; color: %6; }
table { border-collapse: collapse; }
th, td { border: 1px solid %3; padding: 4px 8px; }
hr { border: none; border-top: 1px solid %3; })")
                            .arg(fg.name(), bg.name(), border.name(), accent.name(),
                                 bg.lighter(120).name(), fg.name());
    setHtml(QStringLiteral("<html><head><style>%1</style></head><body>%2</body></html>").arg(css, body));
}

}  // namespace cf
