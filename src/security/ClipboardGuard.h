#pragma once
// Clipboard security: optionally clear the clipboard N seconds after a copy,
// so sensitive text does not sit in the clipboard / clipboard history forever.
// Never keeps a persistent history of copied content (only a transient hash).
#include <QByteArray>
#include <QObject>
#include <QTimer>

class QClipboard;

namespace cf::sec {

class ClipboardGuard : public QObject {
    Q_OBJECT
public:
    explicit ClipboardGuard(QObject* parent = nullptr);

    // seconds == 0 disables the guard.
    void setClearAfterSeconds(int seconds);

    static constexpr const char* kSettingKey = "security.clipboardClearSeconds";

private slots:
    void onClipboardChanged();
    void onTimeout();

private:
    QClipboard* m_clipboard = nullptr;
    QTimer m_timer;
    QByteArray m_lastCopyHash;   // SHA-256 of the text we saw being copied
    bool m_guardActive = false;
    int m_clearSeconds = 0;
};

}  // namespace cf::sec
