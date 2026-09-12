#pragma once
// Authenticated encryption (AES-256-GCM) implementation.
//  - Windows: CNG / BCrypt (OS-provided, FIPS-validated core)
//  - Other platforms: OpenSSL EVP if available at build time (CF_HAVE_OPENSSL)
// SECURITY: keys are never hard-coded; they come from KeyStore (DPAPI-protected).
#include <QByteArray>
#include <QtGlobal>

namespace cf::sec {

class Crypto {
public:
    static constexpr int kKeySize = 32;
    static constexpr int kNonceSize = 12;
    static constexpr int kTagSize = 16;

    static bool available();

    // Cryptographically secure random bytes (BCryptGenRandom / RAND_bytes).
    static bool randomBytes(quint8* buf, int len);

    // out = ciphertext || tag(tagSize)
    static bool gcmEncrypt(const quint8* key, const quint8* nonce,
                           const QByteArray& plaintext, const QByteArray& aad,
                           QByteArray& out);

    // cipherWithTag = ciphertext || tag(tagSize)
    static bool gcmDecrypt(const quint8* key, const quint8* nonce,
                           const QByteArray& cipherWithTag, const QByteArray& aad,
                           QByteArray& outPlain);
};

}  // namespace cf::sec
