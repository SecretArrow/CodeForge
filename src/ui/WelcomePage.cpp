#include "ui/WelcomePage.h"

#include <QClipboard>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QFileInfo>
#include <QToolButton>
#include <QVBoxLayout>

#include "project/RecentManager.h"
#include "ui/Icons.h"

namespace cf {

WelcomePage::WelcomePage(RecentManager* recents, QWidget* parent)
    : QWidget(parent), m_recents(recents)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(48, 32, 48, 32);
    layout->setSpacing(40);

    // Left column: title + actions.
    auto* left = new QVBoxLayout();
    auto* title = new QLabel(QStringLiteral("CodeForge"), this);
    QFont titleFont = title->font();
    titleFont.setPointSize(28);
    titleFont.setBold(true);
    title->setFont(titleFont);
    left->addWidget(title);

    auto* subtitle = new QLabel(tr("A lightweight native code editor for Windows"), this);
    left->addWidget(subtitle);
    left->addSpacing(18);

    auto addButton = [this, left](const QString& text, auto slot) {
        QPushButton* b = new QPushButton(text, this);
        b->setMinimumWidth(220);
        b->setMinimumHeight(34);
        connect(b, &QPushButton::clicked, this, slot);
        left->addWidget(b);
    };
    addButton(tr("Open Folder"), [this]() { emit openFolderRequested(); });
    addButton(tr("Open File"),   [this]() { emit openFileRequested(); });
    addButton(tr("Clone Repository"), [this]() {
        bool ok = false;
        const QString url = QInputDialog::getText(this, tr("Clone Repository"),
            tr("Repository URL:"), QLineEdit::Normal, QString(), &ok);
        if (ok && !url.trimmed().isEmpty()) emit cloneRequested(url.trimmed());
    });
    left->addStretch(1);

    // Right column: recent projects + files.
    auto* right = new QVBoxLayout();
    auto* recentTitle = new QLabel(tr("Recent Projects"), this);
    QFont f = recentTitle->font();
    f.setBold(true);
    recentTitle->setFont(f);
    right->addWidget(recentTitle);

    m_projects = new QListWidget(this);
    m_projects->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_projects, &QListWidget::customContextMenuRequested, this, &WelcomePage::onProjectItemMenu);
    connect(m_projects, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        emit openRecentProject(item->data(Qt::UserRole).toString());
    });
    right->addWidget(m_projects, 2);

    auto* filesTitle = new QLabel(tr("Recent Files"), this);
    filesTitle->setFont(f);
    right->addWidget(filesTitle);
    m_files = new QListWidget(this);
    connect(m_files, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        emit openRecentFile(item->data(Qt::UserRole).toString());
    });
    right->addWidget(m_files, 1);

    auto* clearBtn = new QToolButton(this);
    clearBtn->setText(tr("Clear recent projects"));
    clearBtn->setAutoRaise(true);
    connect(clearBtn, &QToolButton::clicked, this, [this]() {
        m_recents->clearProjects(false);
        refreshRecents();
    });
    right->addWidget(clearBtn, 0, Qt::AlignLeft);

    layout->addLayout(left, 1);
    layout->addLayout(right, 1);

    refreshRecents();
}

void WelcomePage::refreshRecents()
{
    m_projects->clear();
    for (const RecentManager::Entry& e : m_recents->projects()) {
        auto* item = new QListWidgetItem(m_projects);
        item->setText((e.pinned ? QStringLiteral("[pinned]  ") : QString()) +
                      QFileInfo(e.path).fileName() + QStringLiteral("   ") + e.path);
        item->setData(Qt::UserRole, e.path);
    }
    m_files->clear();
    for (const RecentManager::Entry& e : m_recents->recentFiles()) {
        auto* item = new QListWidgetItem(m_files);
        item->setText(e.path);
        item->setData(Qt::UserRole, e.path);
    }
}

void WelcomePage::onProjectItemMenu(const QPoint& pos)
{
    QListWidgetItem* item = m_projects->itemAt(pos);
    if (!item) return;
    const QString path = item->data(Qt::UserRole).toString();
    QMenu menu(this);
    menu.addAction(tr("Open"), this, [this, path]() { emit openRecentProject(path); });
    menu.addSeparator();
    menu.addAction(tr("Pin"), this, [this, path]() {
        m_recents->setPinned(path, true);
        refreshRecents();
    });
    menu.addAction(tr("Remove from Recent"), this, [this, path]() {
        m_recents->removeProject(path);
        refreshRecents();
    });
    menu.exec(m_projects->viewport()->mapToGlobal(pos));
}

}  // namespace cf
