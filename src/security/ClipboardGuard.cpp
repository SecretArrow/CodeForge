#include "security/ClipboardGuard.h"

#include <QClipboard>
#include <QCryptographicHash>
#include <QGuiApplication>

#include "core/Logger.h"

namespace cf::sec {

ClipboardGuard::ClipboardGuard(QObject* parent)
    : QObject(parent)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &ClipboardGuard::onTimeout);
    if (QGuiApplication::instance()) {
        m_clipboard = QGuiApplication::clipboard();
        connect(m_clipboard, &QClipboard::dataChanged, this, &ClipboardGuard::onClipboardChanged);
    }
}

void ClipboardGuard::setClearAfterSeconds(int seconds)
{
    m_clearSeconds = qBound(0, seconds, 3600);
    if (m_clearSeconds == 0) {
        m_timer.stop();
        m_guardActive = false;
    }
}

void ClipboardGuard::onClipboardChanged()
{
    if (!m_clipboard) return;
    const QString text = m_clipboard->text();
    if (text.isEmpty()) { m_guardActive = false; return; }
    m_lastCopyHash = QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha256);
    m_guardActive = true;
    if (m_clearSeconds > 0) m_timer.start(m_clearSeconds * 1000);
}

void ClipboardGuard::onTimeout()
{
    if (!m_clipboard || !m_guardActive) return;
    const QString current = m_clipboard->text();
    const QByteArray hash = QCryptographicHash::hash(current.toUtf8(), QCryptographicHash::Sha256);
    // Only clear if the user has not copied something else in the meantime.
    if (!current.isEmpty() && hash == m_lastCopyHash) {
        m_clipboard->clear();
        m_guardActive = false;
        CF_LOG_INFO(QStringLiteral("ClipboardGuard: clipboard cleared after timeout"));
    }
}

}  // namespace cf::sec
