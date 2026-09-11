#include "ui/ExtensionsPanel.h"

#include <QBoxLayout>
#include <QDesktopServices>
#include <QLabel>
#include <QListWidget>
#include <QToolButton>
#include <QUrl>

#include "extensions/ExtensionHost.h"
#include "ui/Icons.h"

namespace cf {

ExtensionsPanel::ExtensionsPanel(ExtensionHost* host, QWidget* parent)
    : QWidget(parent), m_host(host)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(6);

    auto* head = new QHBoxLayout();
    auto* title = new QLabel(tr("Extensions"), this);
    QFont f = title->font();
    f.setBold(true);
    title->setFont(f);
    head->addWidget(title);
    head->addStretch(1);
    auto* folderBtn = new QToolButton(this);
    folderBtn->setIcon(Icons::icon(Icons::Name::FolderOpen));
    folderBtn->setToolTip(tr("Open Extensions Folder"));
    folderBtn->setAutoRaise(true);
    connect(folderBtn, &QToolButton::clicked, this, &ExtensionsPanel::openExtensionsFolder);
    head->addWidget(folderBtn);
    auto* reloadBtn = new QToolButton(this);
    reloadBtn->setIcon(Icons::icon(Icons::Name::Refresh));
    reloadBtn->setToolTip(tr("Reload Extensions"));
    reloadBtn->setAutoRaise(true);
    connect(reloadBtn, &QToolButton::clicked, this, &ExtensionsPanel::reload);
    head->addWidget(reloadBtn);
    layout->addLayout(head);

    m_list = new QListWidget(this);
    m_list->setWordWrap(true);
    layout->addWidget(m_list, 1);

    reload();
}

void ExtensionsPanel::reload()
{
    m_list->clear();
    for (const ExtensionHost::Entry& e : m_host->entries()) {
        auto* item = new QListWidgetItem(m_list);
        item->setText(QStringLiteral("%1  (%2)\n%3%4")
                          .arg(e.name, e.version, e.description,
                               e.error.isEmpty() ? QString() : QStringLiteral("\nError: %1").arg(e.error)));
        item->setIcon(e.loaded ? Icons::icon(Icons::Name::Puzzle) : Icons::icon(Icons::Name::Warning));
        item->setToolTip(e.loaded ? e.sourceFile : e.error);
    }
}

void ExtensionsPanel::openExtensionsFolder()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_host->extensionsFolder()));
}

}  // namespace cf
