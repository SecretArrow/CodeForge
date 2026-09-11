#pragma once
// Key storage using OS facilities.
//  - Windows: random key, encrypted at rest with DPAPI (CryptProtectData),
//    stored in the user profile -> never hard-coded, never shipped.
//  - Other platforms: key file with 0600 permissions in the user data dir.
// Used to protect recovery snapshots / session caches that may contain
// document content (which is always stored AES-256-GCM encrypted).
#include <QString>

#include "security/SecureBuffer.h"

namespace cf::sec {

class KeyStore {
public:
    // Returns a 32-byte key for the given logical key name ("recovery-v1", ...).
    // Creates + persists a new random key on first use. Returns empty buffer on failure.
    static SecureBuffer getOrCreateKey(const QString& name);

    // Remove stored key material (used by "clear secure data").
    static void deleteKey(const QString& name);

private:
    static QString keyFilePath(const QString& name);
    static bool dpapiProtect(const QByteArray& plain, QByteArray& out);
    static bool dpapiUnprotect(const QByteArray& blob, QByteArray& out);
};

}  // namespace cf::sec
