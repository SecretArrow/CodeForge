#include "ui/StatusBar.h"

#include <QLabel>
#include <QMouseEvent>

#include "core/Common.h"

namespace cf {

StatusBar::StatusBar(QWidget* parent) : QStatusBar(parent)
{
    setSizeGripEnabled(true);
    setObjectName(QStringLiteral("codeforge_statusbar"));

    m_branch = new ClickableLabel(this);
    m_file = new QLabel(this);
    m_position = new ClickableLabel(this);
    m_selection = new QLabel(this);
    m_encoding = new ClickableLabel(this);
    m_eol = new ClickableLabel(this);
    m_language = new ClickableLabel(this);
    m_size = new QLabel(this);
    m_readOnly = new QLabel(this);

    for (QLabel* l : std::initializer_list<QLabel*>{ m_branch, m_file, m_position, m_selection, m_encoding, m_eol, m_language, m_size, m_readOnly })
        l->setContentsMargins(6, 0, 6, 0);

    addWidget(m_branch);
    addWidget(m_file);
    addPermanentWidget(m_position);
    addPermanentWidget(m_selection);
    addPermanentWidget(m_encoding);
    addPermanentWidget(m_eol);
    addPermanentWidget(m_language);
    addPermanentWidget(m_size);
    addPermanentWidget(m_readOnly);

    connect(m_branch, &ClickableLabel::clicked, this, &StatusBar::branchClicked);
    connect(m_position, &ClickableLabel::clicked, this, &StatusBar::positionClicked);
    connect(m_encoding, &ClickableLabel::clicked, this, &StatusBar::encodingClicked);
    connect(m_eol, &ClickableLabel::clicked, this, &StatusBar::lineEndingsClicked);
    connect(m_language, &ClickableLabel::clicked, this, &StatusBar::languageClicked);
}

void StatusBar::setBranch(const QString& branch)
{
    m_branch->setText(branch.isEmpty() ? QString() : QStringLiteral(" %1 ").arg(branch));
}

void StatusBar::setCursorInfo(int line, int col, int selChars, int selLines)
{
    m_position->setText(QStringLiteral("Ln %1, Col %2").arg(line + 1).arg(col + 1));
    if (selChars > 0)
        m_selection->setText(selLines > 1 ? QStringLiteral("%1 lines (%2 chars)").arg(selLines).arg(selChars)
                                          : QStringLiteral("%1 selected").arg(selChars));
    else
        m_selection->clear();
}

void StatusBar::setEncoding(const QString& label)   { m_encoding->setText(label); }
void StatusBar::setLineEndings(const QString& label){ m_eol->setText(label); }
void StatusBar::setLanguage(const QString& label)   { m_language->setText(label); }
void StatusBar::setFileSize(qint64 bytes)           { m_size->setText(formatBytes(bytes)); }
void StatusBar::setReadOnly(bool ro)                { m_readOnly->setText(ro ? QStringLiteral("Read-Only") : QString()); }
void StatusBar::setFilePath(const QString& path)    { m_file->setText(path); }
void StatusBar::clearFile()
{
    m_file->clear();
    m_position->clear();
    m_selection->clear();
    m_encoding->clear();
    m_eol->clear();
    m_language->clear();
    m_size->clear();
    m_readOnly->clear();
}

}  // namespace cf
