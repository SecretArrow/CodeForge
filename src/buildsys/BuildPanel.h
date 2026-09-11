#pragma once
// Build sidebar: CMake project actions + status.
#include <QWidget>

class QLabel;

namespace cf {

class BuildManager;

class BuildPanel : public QWidget {
    Q_OBJECT
public:
    explicit BuildPanel(BuildManager* build, QWidget* parent = nullptr);

private slots:
    void refreshProjectInfo();

private:
    BuildManager* m_build;
    QLabel* m_info;
};

}  // namespace cf
