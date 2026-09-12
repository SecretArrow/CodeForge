#include "security/KeyStore.h"

#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QRegularExpression>

#include "core/AppPaths.h"
#include "core/Logger.h"
#include "security/Crypto.h"

namespace {

bool writeKeyFile(const QString& path, const QByteArray& data)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
#ifndef Q_OS_WIN
    f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
#endif
    if (f.write(data) != data.size()) { f.close(); QFile::remove(path); return false; }
    f.close();
    return true;
}

}  // namespace

#if defined(Q_OS_WIN)
#include <windows.h>
#include <dpapi.h>
#include <wincrypt.h>
#pragma comment(lib, "crypt32.lib")
#endif

namespace cf::sec {

QString KeyStore::keyFilePath(const QString& name)
{
    // Name is a logical identifier, sanitize for filesystem use.
    QString safe = name;
    safe.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_-]")), QStringLiteral("_"));
    Q_ASSERT(!safe.isEmpty());
    return paths::keysDir() + QStringLiteral("/") + safe + QStringLiteral(".key");
}

#if defined(Q_OS_WIN)
bool KeyStore::dpapiProtect(const QByteArray& plain, QByteArray& out)
{
    DATA_BLOB in, outBlob;
    in.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(plain.constData()));
    in.cbData = DWORD(plain.size());
    if (!CryptProtectData(&in, L"CodeForgeKey", nullptr, nullptr, nullptr, 0, &outBlob))
        return false;
    out = QByteArray(reinterpret_cast<const char*>(outBlob.pbData), int(outBlob.cbData));
    LocalFree(outBlob.pbData);
    return true;
}

bool KeyStore::dpapiUnprotect(const QByteArray& blob, QByteArray& out)
{
    DATA_BLOB in, outBlob;
    in.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(blob.constData()));
    in.cbData = DWORD(blob.size());
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, 0, &outBlob))
        return false;
    out = QByteArray(reinterpret_cast<const char*>(outBlob.pbData), int(outBlob.cbData));
    LocalFree(outBlob.pbData);
    return true;
}
#endif

SecureBuffer KeyStore::getOrCreateKey(const QString& name)
{
    SecureBuffer empty;
    if (!Crypto::available()) {
        CF_LOG_ERROR(QStringLiteral("KeyStore: no crypto backend available"));
        return empty;
    }

    const QString path = keyFilePath(name);
    QFile f(path);
    QByteArray stored;

    if (f.exists() && f.open(QIODevice::ReadOnly)) {
        stored = f.readAll();
        f.close();
#if defined(Q_OS_WIN)
        QByteArray plain;
        if (!dpapiUnprotect(stored, plain) || plain.size() != Crypto::kKeySize) {
            CF_LOG_ERROR(QStringLiteral("KeyStore: failed to unprotect key %1 (profile change?)").arg(name));
            return empty;
        }
        SecureBuffer key;
        if (!key.resize(Crypto::kKeySize)) return empty;
        memcpy(key.data(), plain.constData(), Crypto::kKeySize);
        secureZero(plain.data(), size_t(plain.size()));
        return key;
#else
        if (stored.size() == Crypto::kKeySize) {
            SecureBuffer key;
            if (!key.resize(Crypto::kKeySize)) return empty;
            memcpy(key.data(), stored.constData(), Crypto::kKeySize);
            return key;
        }
        CF_LOG_ERROR(QStringLiteral("KeyStore: corrupt key file %1").arg(name));
        return empty;
#endif
    }

    // Create a fresh random key.
    SecureBuffer key;
    if (!key.resize(Crypto::kKeySize)) return empty;
    if (!Crypto::randomBytes(key.data(), Crypto::kKeySize)) {
        CF_LOG_ERROR(QStringLiteral("KeyStore: CSPRNG failure"));
        return empty;
    }

#if defined(Q_OS_WIN)
    QByteArray plain(reinterpret_cast<const char*>(key.data()), Crypto::kKeySize);
    QByteArray protectedBlob;
    if (!dpapiProtect(plain, protectedBlob)) {
        CF_LOG_ERROR(QStringLiteral("KeyStore: DPAPI protect failed"));
        return empty;
    }
    if (!writeKeyFile(path, protectedBlob)) return empty;
#else
    if (!writeKeyFile(path, key.toByteArray())) return empty;
#endif
    CF_LOG_INFO(QStringLiteral("KeyStore: created new key '%1'").arg(name));
    return key;
}

void KeyStore::deleteKey(const QString& name)
{
    QFile::remove(keyFilePath(name));
}

}  // namespace cf::sec
