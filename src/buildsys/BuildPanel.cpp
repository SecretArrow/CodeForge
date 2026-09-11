#include "buildsys/BuildPanel.h"

#include <QBoxLayout>
#include <QFileInfo>
#include <QLabel>
#include <QPushButton>

#include "buildsys/BuildManager.h"
#include "ui/Icons.h"

namespace cf {

BuildPanel::BuildPanel(BuildManager* build, QWidget* parent)
    : QWidget(parent), m_build(build)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(6);

    auto* title = new QLabel(tr("Build"), this);
    QFont f = title->font();
    f.setBold(true);
    title->setFont(f);
    layout->addWidget(title);

    m_info = new QLabel(this);
    m_info->setWordWrap(true);
    layout->addWidget(m_info);

    auto addButton = [this, &layout](const QString& text, auto slot) {
        QPushButton* b = new QPushButton(text, this);
        connect(b, &QPushButton::clicked, this, slot);
        layout->addWidget(b);
    };
    addButton(tr("Configure"), [this]() { m_build->configure(); });
    addButton(tr("Build"), [this]() { m_build->build(); });
    addButton(tr("Rebuild"), [this]() { m_build->rebuild(); });
    addButton(tr("Clean"), [this]() { m_build->clean(); });
    addButton(tr("Run"), [this]() { m_build->runExecutable(m_build->detectExecutable()); });
    layout->addStretch(1);

    connect(m_build, &BuildManager::projectChanged, this, [this](bool isCMake) { Q_UNUSED(isCMake); refreshProjectInfo(); });
    refreshProjectInfo();
}

void BuildPanel::refreshProjectInfo()
{
    if (m_build->isCMakeProject())
        m_info->setText(tr("CMake project detected.\nBuild dir: %1").arg(m_build->buildDir()));
    else
        m_info->setText(tr("No CMakeLists.txt found in the workspace root.\nBuild actions are disabled."));
    setEnabled(m_build->isCMakeProject());
}

}  // namespace cf
