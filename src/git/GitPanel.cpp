#include "git/GitPanel.h"

#include <QComboBox>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "git/GitClient.h"
#include "ui/Icons.h"

namespace cf {

GitPanel::GitPanel(GitClient* git, QWidget* parent)
    : QWidget(parent), m_git(git)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(6);

    auto* branchRow = new QHBoxLayout();
    m_branchBox = new QComboBox(this);
    m_branchBox->setToolTip(tr("Switch branch"));
    branchRow->addWidget(m_branchBox, 1);
    auto* refreshBtn = new QToolButton(this);
    refreshBtn->setIcon(Icons::icon(Icons::Name::Refresh));
    refreshBtn->setToolTip(tr("Refresh"));
    refreshBtn->setAutoRaise(true);
    branchRow->addWidget(refreshBtn);
    layout->addLayout(branchRow);

    m_changes = new QTreeWidget(this);
    m_changes->setHeaderLabels({ tr("Changes"), tr("Status") });
    m_changes->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_changes->setRootIsDecorated(false);
    m_changes->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(m_changes, 1);

    m_summary = new QLineEdit(this);
    m_summary->setPlaceholderText(tr("Commit summary (required)"));
    layout->addWidget(m_summary);
    m_message = new QPlainTextEdit(this);
    m_message->setPlaceholderText(tr("Extended description (optional)"));
    m_message->setFixedHeight(60);
    layout->addWidget(m_message);
    m_commitBtn = new QPushButton(tr("Commit"), this);
    layout->addWidget(m_commitBtn);

    connect(refreshBtn, &QToolButton::clicked, this, [this]() {
        if (!m_git->repoRoot().isEmpty()) m_git->refresh(m_git->repoRoot());
    });
    connect(m_git, &GitClient::stateChanged, this, &GitPanel::onStateChanged);
    connect(m_git, &GitClient::statusUpdated, this, [this](const QHash<QString, QString>& relState) {
        m_status = relState;
        refreshView();
    });
    connect(m_git, &GitClient::commandFinished, this, [this](bool ok, const QString& summary) {
        if (!ok) QMessageBox::warning(this, tr("Git"), tr("Command failed: %1").arg(summary));
    });
    connect(m_commitBtn, &QPushButton::clicked, this, &GitPanel::commit);
    connect(m_changes, &QTreeWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QTreeWidgetItem* item = m_changes->itemAt(pos);
        if (!item) return;
        const Section section = Section(item->data(0, Qt::UserRole).toInt());
        const QString path = item->data(0, Qt::UserRole + 1).toString();
        QMenu menu(this);
        if (section == Section::Untracked || section == Section::Unstaged)
            menu.addAction(tr("Stage"), this, [this, item]() { stageItem(item, false); });
        if (section == Section::Staged)
            menu.addAction(tr("Unstage"), this, [this, item]() { stageItem(item, true); });
        if (section == Section::Unstaged) {
            menu.addSeparator();
            menu.addAction(tr("Discard Changes"), this, [this, item]() { discardItem(item); });
            menu.addSeparator();
            menu.addAction(tr("View Diff"), this, [this, path]() {
                emit openDiffRequested(path, m_git->fileDiff(path, false));
            });
        }
        menu.exec(m_changes->viewport()->mapToGlobal(pos));
    });
    connect(m_branchBox, &QComboBox::currentTextChanged, this, [this](const QString& branch) {
        if (m_updatingBranches || branch.isEmpty()) return;
        m_git->checkoutBranch(branch);
    });

    onStateChanged();
}

void GitPanel::onStateChanged()
{
    const bool repo = m_git->isRepository();
    m_branchBox->setEnabled(repo);
    m_commitBtn->setEnabled(repo);
    m_summary->setEnabled(repo);
    m_message->setEnabled(repo);

    m_updatingBranches = true;
    m_branchBox->clear();
    if (repo) {
        m_branchBox->addItems(m_git->branches());
        m_branchBox->setCurrentText(m_git->currentBranch());
    }
    m_updatingBranches = false;
}

void GitPanel::refreshView()
{
    m_changes->clear();
    if (!m_git->isRepository()) {
        m_changes->addTopLevelItem(new QTreeWidgetItem({ tr("Not a git repository") }));
        return;
    }

    auto addSection = [this](const QString& title, Section section,
                             const QList<QPair<QString, QString>>& entries) {
        if (entries.isEmpty()) return;
        auto* head = new QTreeWidgetItem(m_changes, { title });
        head->setFirstColumnSpanned(true);
        QFont f = head->font(0);
        f.setBold(true);
        head->setFont(0, f);
        head->setData(0, Qt::UserRole, int(section));
        for (const auto& [path, state] : entries) {
            auto* item = new QTreeWidgetItem(head, { path, state });
            item->setData(0, Qt::UserRole, int(section));
            item->setData(0, Qt::UserRole + 1, path);
        }
    };

    QList<QPair<QString, QString>> staged, unstaged, untracked;
    for (auto it = m_status.constBegin(); it != m_status.constEnd(); ++it) {
        const QString state = it.value();
        const QString indexState = state.left(1);
        const QString workState = state.mid(1, 1);
        if (indexState != QLatin1String(" ") && indexState != QLatin1String("?"))
            staged.append({ it.key(), indexState });
        if (workState != QLatin1String(" ") && workState != QLatin1String("?"))
            unstaged.append({ it.key(), workState });
        if (indexState == QLatin1String("?"))
            untracked.append({ it.key(), QStringLiteral("U") });
    }

    addSection(tr("Staged Changes"), Section::Staged, staged);
    addSection(tr("Changes"), Section::Unstaged, unstaged);
    addSection(tr("Untracked"), Section::Untracked, untracked);

    if (m_changes->topLevelItemCount() == 0)
        m_changes->addTopLevelItem(new QTreeWidgetItem({ tr("Working tree clean") }));
}

void GitPanel::stageItem(QTreeWidgetItem* item, bool unstage)
{
    if (!item->parent()) return;
    const QString path = item->data(0, Qt::UserRole + 1).toString();
    if (unstage) m_git->unstage({ path });
    else m_git->stage({ path });
}

void GitPanel::discardItem(QTreeWidgetItem* item)
{
    if (!item->parent()) return;
    const QString path = item->data(0, Qt::UserRole + 1).toString();
    if (QMessageBox::question(this, tr("Discard Changes"),
        tr("Discard all local changes to '%1'?\n\nThis cannot be undone.").arg(path),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes)
        return;
    m_git->discard({ path });
}

void GitPanel::commit()
{
    const QString summary = m_summary->text().trimmed();
    if (summary.isEmpty()) {
        QMessageBox::information(this, tr("Commit"), tr("Please enter a commit summary."));
        return;
    }
    QString message = summary;
    if (!m_message->toPlainText().trimmed().isEmpty())
        message += QStringLiteral("\n\n") + m_message->toPlainText().trimmed();
    m_git->commit(message);
    m_summary->clear();
    m_message->clear();
}

}  // namespace cf
