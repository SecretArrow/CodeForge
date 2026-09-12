#include <QtTest>

#include "security/Crypto.h"
#include "security/SecureBuffer.h"

using namespace cf;

class TestCrypto : public QObject {
    Q_OBJECT
private slots:
    void availability()
    {
        if (!sec::Crypto::available())
            QSKIP("No crypto backend on this platform");
    }

    void roundTrip()
    {
        if (!sec::Crypto::available()) QSKIP("no crypto backend");
        sec::SecureBuffer key;
        QVERIFY(key.resize(sec::Crypto::kKeySize));
        QVERIFY(sec::Crypto::randomBytes(key.data(), sec::Crypto::kKeySize));

        QByteArray nonce(sec::Crypto::kNonceSize, Qt::Uninitialized);
        QVERIFY(sec::Crypto::randomBytes(reinterpret_cast<quint8*>(nonce.data()), sec::Crypto::kNonceSize));

        const QByteArray plain = QStringLiteral("document content — jangan disimpan plaintext").toUtf8();
        const QByteArray aad = QByteArrayLiteral("codeforge-recovery");

        QByteArray cipher;
        QVERIFY(sec::Crypto::gcmEncrypt(key.data(), reinterpret_cast<const quint8*>(nonce.constData()), plain, aad, cipher));
        QVERIFY(cipher.size() == plain.size() + sec::Crypto::kTagSize);
        QVERIFY(cipher != plain);

        QByteArray out;
        QVERIFY(sec::Crypto::gcmDecrypt(key.data(), reinterpret_cast<const quint8*>(nonce.constData()), cipher, aad, out));
        QCOMPARE(out, plain);
    }

    void tamperDetection()
    {
        if (!sec::Crypto::available()) QSKIP("no crypto backend");
        sec::SecureBuffer key;
        QVERIFY(key.resize(sec::Crypto::kKeySize));
        QVERIFY(sec::Crypto::randomBytes(key.data(), sec::Crypto::kKeySize));

        QByteArray nonce(sec::Crypto::kNonceSize, Qt::Uninitialized);
        QVERIFY(sec::Crypto::randomBytes(reinterpret_cast<quint8*>(nonce.data()), sec::Crypto::kNonceSize));

        QByteArray cipher;
        QVERIFY(sec::Crypto::gcmEncrypt(key.data(), reinterpret_cast<const quint8*>(nonce.constData()),
                                        QByteArrayLiteral("secret"), QByteArrayLiteral("aad"), cipher));
        cipher[0] = static_cast<char>(cipher.at(0) ^ 0x01);   // flip a bit

        QByteArray out;
        QVERIFY2(!sec::Crypto::gcmDecrypt(key.data(), reinterpret_cast<const quint8*>(nonce.constData()), cipher,
                                          QByteArrayLiteral("aad"), out),
                 "GCM must reject tampered ciphertext");
    }

    void zeroize()
    {
        sec::SecureBuffer buf;
        QVERIFY(buf.resize(64));
        memset(buf.data(), 0xAB, 64);
        buf.zeroize();
        const quint8* p = buf.data();
        for (int i = 0; i < 64; ++i) QCOMPARE(int(p[i]), 0);
    }
};

QTEST_GUILESS_MAIN(TestCrypto)
#include "test_crypto.moc"
