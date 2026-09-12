#pragma once
// Markdown -> HTML converter (Qt-only, no external dependency).
// Supports: ATX headings, fenced code blocks, tables, blockquotes,
// ordered/unordered lists (one nesting level via indentation), task lists,
// horizontal rules, paragraphs, and inline styles (bold, italic, strike,
// inline code, links, images, autolinks).
#include <QString>

namespace cf {
namespace markdown {

// Converts markdown source to an HTML fragment (no <html>/<body> wrapper).
QString toHtml(const QString& source);

// Escapes HTML special characters.
QString escapeHtml(const QString& text);

}  // namespace markdown
}  // namespace cf
