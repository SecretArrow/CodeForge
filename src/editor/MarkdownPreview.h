#pragma once
// MarkdownPreview: live rendered preview of the active markdown document
// (QTextBrowser based, styled from the current theme, debounced updates).
#include <QTextBrowser>
#include <QTimer>

#include "core/TextDocument.h"

namespace cf {

class MarkdownPreview : public QTextBrowser {
    Q_OBJECT
public:
    explicit MarkdownPreview(QWidget* parent = nullptr);

    void setDocument(TextDocument* doc);   // nullptr -> placeholder

private slots:
    void render();

private:
    QPointer<TextDocument> m_doc;
    QTimer m_debounce;
};

}  // namespace cf
