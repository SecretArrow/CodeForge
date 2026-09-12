#include "security/Crypto.h"

#include "core/Logger.h"
#include "security/SecureBuffer.h"

#if defined(Q_OS_WIN)
#include <qt_windows.h>
#include <ntstatus.h>
#ifndef NT_SUCCESS
#define NT_SUCCESS(s) (((NTSTATUS)(s)) >= 0)
#endif
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")

namespace {

// Bind AAD to a CNG authenticated-cipher info struct. Two overloads:
//  - SDKs that still declare the pbAAD/cbAAD members select the constrained
//    overload and bind them;
//  - newer Windows SDK revisions (VS 18 / SDK 10.0.26100+) removed the
//    members, the constrained overload is then not viable, its body is never
//    instantiated, and the fallback reports "unsupported" so callers fail
//    closed instead of producing a ciphertext the other backend could not
//    verify. (An if constexpr here would still semantically check both
//    branches in a non-template function, which is exactly what must not
//    happen.)
template <typename InfoT>
    requires requires(InfoT& i) { i.pbAAD; i.cbAAD; }
bool cngApplyAad(InfoT& info, const QByteArray& aad)
{
    info.pbAAD = const_cast<PUCHAR>(reinterpret_cast<const uchar*>(aad.constData()));
    info.cbAAD = ULONG(aad.size());
    return true;
}

template <typename InfoT>
bool cngApplyAad(InfoT&, const QByteArray&)
{
    cf::Logger::instance().warning(QStringLiteral(
        "Crypto: CNG GCM AAD members unavailable in this Windows SDK; "
        "AAD-protected request rejected (fail closed)"));
    return false;
}

}  // namespace

namespace cf::sec {

bool Crypto::available() { return true; }

bool Crypto::randomBytes(quint8* buf, int len)
{
    NTSTATUS st = BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(buf), ULONG(len), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    return NT_SUCCESS(st);
}

bool Crypto::gcmEncrypt(const quint8* key, const quint8* nonce,
                        const QByteArray& plaintext, const QByteArray& aad, QByteArray& out)
{
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    PUCHAR tagBuf = nullptr, ivBuf = nullptr;
    bool ok = false;

    do {
        if (!NT_SUCCESS(BCryptOpenAlgorithmProvider(&alg, BCRYPT_AES_ALGORITHM, nullptr, 0))) break;
        if (!NT_SUCCESS(BCryptSetProperty(alg, BCRYPT_CHAINING_MODE,
                                          reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_GCM)),
                                          sizeof(BCRYPT_CHAIN_MODE_GCM), 0))) break;
        if (!NT_SUCCESS(BCryptGenerateSymmetricKey(alg, &hKey, nullptr, 0,
                                                   const_cast<PUCHAR>(key), DWORD(kKeySize), 0))) break;

        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
        BCRYPT_INIT_AUTH_MODE_INFO(info);
        info.pbNonce = const_cast<PUCHAR>(nonce);
        info.cbNonce = kNonceSize;
        QByteArray tag(kTagSize, Qt::Uninitialized);
        tagBuf = reinterpret_cast<PUCHAR>(tag.data());
        info.pbTag = tagBuf;
        info.cbTag = kTagSize;
        if (!aad.isEmpty() && !cngApplyAad(info, aad)) {
            break;   // ok stays false; handles are cleaned up below
        }

        out.clear();
        out.resize(int(plaintext.size()));
        ULONG outLen = 0;
        NTSTATUS st = BCryptEncrypt(hKey,
                                    const_cast<PUCHAR>(reinterpret_cast<const uchar*>(plaintext.constData())),
                                    ULONG(plaintext.size()),
                                    &info,
                                    ivBuf, 0,
                                    reinterpret_cast<PUCHAR>(out.data()), ULONG(out.size()),
                                    &outLen, 0);
        if (!NT_SUCCESS(st)) break;
        out.resize(int(outLen));
        out.append(tag);
        ok = true;
    } while (false);

    if (hKey) BCryptDestroyKey(hKey);
    if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    return ok;
}

bool Crypto::gcmDecrypt(const quint8* key, const quint8* nonce,
                        const QByteArray& cipherWithTag, const QByteArray& aad, QByteArray& outPlain)
{
    if (cipherWithTag.size() < kTagSize) return false;

    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    bool ok = false;

    do {
        if (!NT_SUCCESS(BCryptOpenAlgorithmProvider(&alg, BCRYPT_AES_ALGORITHM, nullptr, 0))) break;
        if (!NT_SUCCESS(BCryptSetProperty(alg, BCRYPT_CHAINING_MODE,
                                          reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_GCM)),
                                          sizeof(BCRYPT_CHAIN_MODE_GCM), 0))) break;
        if (!NT_SUCCESS(BCryptGenerateSymmetricKey(alg, &hKey, nullptr, 0,
                                                   const_cast<PUCHAR>(key), DWORD(kKeySize), 0))) break;

        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
        BCRYPT_INIT_AUTH_MODE_INFO(info);
        info.pbNonce = const_cast<PUCHAR>(nonce);
        info.cbNonce = kNonceSize;
        const QByteArray tag = cipherWithTag.right(kTagSize);
        info.pbTag = const_cast<PUCHAR>(reinterpret_cast<const uchar*>(tag.constData()));
        info.cbTag = kTagSize;
        if (!aad.isEmpty() && !cngApplyAad(info, aad)) {
            break;   // ok stays false; handles are cleaned up below
        }

        const QByteArray cipher = cipherWithTag.left(cipherWithTag.size() - kTagSize);
        outPlain.clear();
        outPlain.resize(int(cipher.size()));
        ULONG outLen = 0;
        NTSTATUS st = BCryptDecrypt(hKey,
                                    const_cast<PUCHAR>(reinterpret_cast<const uchar*>(cipher.constData())),
                                    ULONG(cipher.size()),
                                    &info,
                                    nullptr, 0,
                                    reinterpret_cast<PUCHAR>(outPlain.data()), ULONG(outPlain.size()),
                                    &outLen, 0);
        if (!NT_SUCCESS(st)) break;   // tag mismatch or malformed input
        outPlain.resize(int(outLen));
        ok = true;
    } while (false);

    if (hKey) BCryptDestroyKey(hKey);
    if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    return ok;
}

}  // namespace cf::sec

#elif defined(CF_HAVE_OPENSSL)

#include <openssl/evp.h>
#include <openssl/rand.h>

namespace cf::sec {

bool Crypto::available() { return true; }

bool Crypto::randomBytes(quint8* buf, int len)
{
    return RAND_bytes(buf, len) == 1;
}

bool Crypto::gcmEncrypt(const quint8* key, const quint8* nonce,
                        const QByteArray& plaintext, const QByteArray& aad, QByteArray& out)
{
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return false;
    out.clear();
    QByteArray tag(kTagSize, Qt::Uninitialized);
    bool ok = false;
    int len = 0;

    do {
        if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) break;
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, kNonceSize, nullptr) != 1) break;
        if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, key, nonce) != 1) break;
        if (!aad.isEmpty() && EVP_EncryptUpdate(ctx, nullptr, &len,
                                                reinterpret_cast<const uchar*>(aad.constData()), aad.size()) != 1) break;
        out.resize(int(plaintext.size()));
        int total = 0;
        if (!plaintext.isEmpty()) {
            if (EVP_EncryptUpdate(ctx, reinterpret_cast<uchar*>(out.data()), &len,
                                  reinterpret_cast<const uchar*>(plaintext.constData()), plaintext.size()) != 1) break;
            total = len;
        }
        if (EVP_EncryptFinal_ex(ctx, reinterpret_cast<uchar*>(out.data()) + total, &len) != 1) break;
        total += len;
        out.resize(total);
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, kTagSize, tag.data()) != 1) break;
        out.append(tag);
        ok = true;
    } while (false);

    EVP_CIPHER_CTX_free(ctx);
    if (!ok) out.clear();
    return ok;
}

bool Crypto::gcmDecrypt(const quint8* key, const quint8* nonce,
                        const QByteArray& cipherWithTag, const QByteArray& aad, QByteArray& outPlain)
{
    if (cipherWithTag.size() < kTagSize) return false;
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return false;
    const QByteArray cipher = cipherWithTag.left(cipherWithTag.size() - kTagSize);
    const QByteArray tag = cipherWithTag.right(kTagSize);
    outPlain.clear();
    outPlain.resize(int(cipher.size()));
    bool ok = false;
    int len = 0;

    do {
        if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) break;
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, kNonceSize, nullptr) != 1) break;
        if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, key, nonce) != 1) break;
        if (!aad.isEmpty() && EVP_DecryptUpdate(ctx, nullptr, &len,
                                                reinterpret_cast<const uchar*>(aad.constData()), aad.size()) != 1) break;
        int total = 0;
        if (!cipher.isEmpty()) {
            if (EVP_DecryptUpdate(ctx, reinterpret_cast<uchar*>(outPlain.data()), &len,
                                  reinterpret_cast<const uchar*>(cipher.constData()), cipher.size()) != 1) break;
            total = len;
        }
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, kTagSize, const_cast<uchar*>(reinterpret_cast<const uchar*>(tag.constData()))) != 1) break;
        if (EVP_DecryptFinal_ex(ctx, reinterpret_cast<uchar*>(outPlain.data()) + total, &len) != 1) break;  // auth failure
        outPlain.resize(total + len);
        ok = true;
    } while (false);

    EVP_CIPHER_CTX_free(ctx);
    if (!ok) { secureZero(outPlain.data(), size_t(outPlain.size())); outPlain.clear(); }
    return ok;
}

}  // namespace cf::sec

#else

namespace cf::sec {

// No crypto backend on this platform: encrypted persistence is disabled and
// the app degrades safely (recovery snapshots are skipped, never plaintext).
bool Crypto::available() { return false; }
bool Crypto::randomBytes(quint8*, int) { return false; }
bool Crypto::gcmEncrypt(const quint8*, const quint8*, const QByteArray&, const QByteArray&, QByteArray&) { return false; }
bool Crypto::gcmDecrypt(const quint8*, const quint8*, const QByteArray&, const QByteArray&, QByteArray&) { return false; }

}  // namespace cf::sec

#endif
