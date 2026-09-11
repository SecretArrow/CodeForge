#pragma once
// Session persistence: workspace, tabs/groups layout, UI state, window
// geometry. Content of unsaved buffers lives (encrypted) in DocumentManager's
// recovery store, never in session.json.
#include <QJsonObject>
#include <QObject>
#include <QRect>

namespace cf {

class SessionManager : public QObject {
    Q_OBJECT
public:
    explicit SessionManager(QObject* parent = nullptr);

    void saveMainWindow(class MainWindow* window);
    bool hasSession() const;
    QJsonObject loadSession() const;

    void clear();
};

}  // namespace cf
