#pragma once
// Extensions sidebar view: lists built-in packs + loaded plugins.
#include <QWidget>

class QListWidget;

namespace cf {

class ExtensionHost;

class ExtensionsPanel : public QWidget {
    Q_OBJECT
public:
    explicit ExtensionsPanel(ExtensionHost* host, QWidget* parent = nullptr);

    void reload();

private slots:
    void openExtensionsFolder();

private:
    ExtensionHost* m_host;
    QListWidget* m_list;
};

}  // namespace cf
