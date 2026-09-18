// CodeForge unit tests: Assembly Core (src/asm/*.asm + portable fallbacks).
//
// On Windows-x64/MSVC builds these tests execute the REAL .asm machine code;
// on Linux/macOS they execute the bit-exact C++ fallbacks in asmcore.h.
// Oracles used: published check values, the NIST SHA-256 "abc" vector,
// QCryptographicHash, and naive reference implementations on random data.
#include <QtTest>
#include <QRandomGenerator>
#include <QCryptographicHash>

#include <algorithm>
#include <cstring>

#include "asm/asmcore.h"

using namespace cf::asmcore;

namespace {

std::uint32_t crc32cReference(std::uint32_t seed, const std::uint8_t* p, std::size_t n)
{
    std::uint32_t crc = seed ^ 0xFFFFFFFFu;
    for (std::size_t i = 0; i < n; ++i) {
        crc ^= p[i];
        for (int k = 0; k < 8; ++k)
            crc = (crc >> 1) ^ (0x82F63B78u & (0u - (crc & 1u)));
    }
    return crc ^ 0xFFFFFFFFu;
}

std::int64_t memSearchReference(const std::uint8_t* hay, std::size_t hayLen,
                                const std::uint8_t* needle, std::size_t needleLen)
{
    if (needleLen == 0) return 0;
    if (needleLen > hayLen) return -1;
    const std::size_t last = hayLen - needleLen;
    for (std::size_t i = 0; i <= last; ++i) {
        std::size_t j = 0;
        while (j < needleLen && hay[i + j] == needle[j]) ++j;
        if (j == needleLen) return static_cast<std::int64_t>(i);
    }
    return -1;
}

QByteArray sha256ViaAsmCore(const QByteArray& msg)
{
    std::uint32_t st[8];
    for (int i = 0; i < 8; ++i) st[i] = Sha256IV::words[i];

    QByteArray m = msg;
    const quint64 bitLen = quint64(m.size()) * 8ull;
    m.append(char(0x80));
    while (m.size() % 64 != 56) m.append(char(0x00));
    for (int i = 7; i >= 0; --i) m.append(char((bitLen >> (8 * i)) & 0xFF));

    for (int off = 0; off < m.size(); off += 64)
        sha256Compress(st, reinterpret_cast<const std::uint8_t*>(m.constData()) + off);

    QByteArray out(32, char(0));
    for (int i = 0; i < 8; ++i) {
        out[4*i + 0] = char((st[i] >> 24) & 0xFF);
        out[4*i + 1] = char((st[i] >> 16) & 0xFF);
        out[4*i + 2] = char((st[i] >> 8) & 0xFF);
        out[4*i + 3] = char(st[i] & 0xFF);
    }
    return out;
}

}  // namespace

class TestAsmCore : public QObject
{
    Q_OBJECT

private slots:
    void secureZeroVariousSizes()
    {
        const int sizes[] = {0, 1, 2, 7, 8, 9, 15, 16, 17, 63, 64, 65, 255, 4096};
        for (int size : sizes) {
            QByteArray buf(std::max(size, 1), char(0xAA));
            secureZero(buf.data(), std::size_t(size));
            for (int i = 0; i < size; ++i)
                QVERIFY2(buf[i] == char(0), qPrintable(QString("offset %1 not zeroed (size %2)").arg(i).arg(size)));
        }
    }

    void crc32cCheckValue()
    {
        // Published CRC-32C check value for "123456789".
        const QByteArray msg = "123456789";
        QCOMPARE(crc32c(0, msg.constData(), std::size_t(msg.size())), 0xE3069283u);
        QCOMPARE(crc32cReference(0, reinterpret_cast<const std::uint8_t*>(msg.constData()),
                                 std::size_t(msg.size())), 0xE3069283u);
        // Empty input.
        QCOMPARE(crc32c(0, msg.constData(), 0), 0u);
        QCOMPARE(crc32cReference(0, nullptr, 0), 0u);
    }

    void crc32cChainingAndDifferential()
    {
        // Chaining: crc32c(crc32c(0,a), b) == crc32c(0, a+b).
        const QByteArray a = "CodeForge ";
        const QByteArray b = "Assembly Core";
        const std::uint32_t c1 = crc32c(crc32c(0, a.constData(), std::size_t(a.size())),
                                        b.constData(), std::size_t(b.size()));
        const QByteArray ab = a + b;
        QCOMPARE(c1, crc32c(0, ab.constData(), std::size_t(ab.size())));

        // Differential vs the bitwise reference on random buffers.
        QRandomGenerator rng(20260918u);
        for (int iter = 0; iter < 300; ++iter) {
            const int len = int(rng.bounded(0, 3000));
            QByteArray buf(len, char(0));
            for (int i = 0; i < len; ++i) buf[i] = char(rng.bounded(0, 256));
            const std::uint32_t got = crc32c(0, buf.constData(), std::size_t(len));
            const std::uint32_t want = crc32cReference(0,
                reinterpret_cast<const std::uint8_t*>(buf.constData()), std::size_t(len));
            if (got != want)
                QFAIL(qPrintable(QString("crc32c mismatch at iter %1 len %2: %3 vs %4")
                                     .arg(iter).arg(len).arg(got, 8, 16).arg(want, 8, 16)));
        }
    }

    void utf8ValidCases()
    {
        // Valid sequences (pure ASCII, 2/3/4-byte, min/max code points).
        const char* validCases[] = {
            "", "abc", "hello world 123",
            "h\xc3\xa9llo w\xc3\xb6rld",                    // 2-byte
            "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e",         // 3-byte (Japanese)
            "\xf0\x9f\x99\x82",                             // 4-byte emoji
            "\x7f",                                         // max ASCII
            "\xc2\x80", "\xdf\xbf",                         // 2-byte min/max
            "\xe0\xa0\x80", "\xef\xbf\xbf",                 // 3-byte min/max
            "\xf0\x90\x80\x80", "\xf4\x8f\xbf\xbf",         // 4-byte min (U+10000) / max (U+10FFFF)
            "mix\xc3\xa9\xe6\x97\xa5\xf0\x9f\x99\x82end",
        };
        for (const char* v : validCases)
            QVERIFY2(utf8Valid(v, std::strlen(v)), qPrintable(QString("should be valid: %1").arg(QString(v))));

        // Invalid sequences: truncated, overlong, surrogates, out of range.
        const char* invalidCases[] = {
            "\x80",                     // lone continuation
            "\xc0\xaf",                 // overlong 2-byte
            "\xc1\xbf",                 // overlong 2-byte
            "\xc3",                     // truncated 2-byte
            "\xe0\x80\x80",             // overlong 3-byte
            "\xe0\x9f\xbf",             // overlong 3-byte
            "\xed\xa0\x80",             // UTF-16 surrogate half
            "\xed\xbf\xbf",             // surrogate range
            "\xf0\x8f\xbf\xbf",         // overlong 4-byte
            "\xf4\x90\x80\x80",         // above U+10FFFF
            "\xf5\x80\x80\x80",         // lead byte beyond F4
            "\xff",                     // invalid lead
            "abc\xc3",                  // truncated at end of input
            "\xe6\x97\xa5\xe6\x9c",     // truncated 3-byte
        };
        for (const char* v : invalidCases)
            QVERIFY2(!utf8Valid(v, std::strlen(v)), qPrintable(QString("should be invalid: %1").arg(QString(v))));
    }

    void memSearchCases()
    {
        const std::uint8_t hay[] = "the quick brown fox jumps over the lazy dog";
        const std::size_t hayLen = std::strlen(reinterpret_cast<const char*>(hay));
        QCOMPARE(memSearch(hay, hayLen, hay, hayLen), std::int64_t(0));          // whole
        const std::uint8_t quick[] = "quick";
        QCOMPARE(memSearch(hay, hayLen, quick, 5), std::int64_t(4));             // middle
        const std::uint8_t dog[] = "dog";
        QCOMPARE(memSearch(hay, hayLen, dog, 3), std::int64_t(hayLen - 3));      // end
        const std::uint8_t cat[] = "cat!";
        QCOMPARE(memSearch(hay, hayLen, cat, 4), std::int64_t(-1));              // absent
        const std::uint8_t dogs[] = "dogs";
        QCOMPARE(memSearch(hay, hayLen, dogs, 4), std::int64_t(-1));             // prefix matches, longer needle does not
        const std::uint8_t big[] = "the quick brown fox jumps over the lazy doh";
        QCOMPARE(memSearch(hay, hayLen, big, hayLen), std::int64_t(-1));         // needle == hayLen, last byte differs
        QCOMPARE(memSearch(hay, hayLen, nullptr, 0), std::int64_t(0));           // empty needle
        QCOMPARE(memSearch(nullptr, 0, nullptr, 0), std::int64_t(0));            // both empty
        const std::uint8_t z[] = "z";
        QCOMPARE(memSearch(hay, hayLen, z, 1), std::int64_t(37));                // single byte
        // Repeated-overlap pattern.
        const std::uint8_t aaab[] = "aaaab";
        const std::uint8_t ab[] = "aab";
        QCOMPARE(memSearch(aaab, 5, ab, 3), std::int64_t(1));
    }

    void memSearchDifferential()
    {
        QRandomGenerator rng(4242u);
        std::uint8_t hay[300];
        std::uint8_t needle[48];
        for (int iter = 0; iter < 400; ++iter) {
            const int hayLen = int(rng.bounded(1, 300));
            const int needleLen = int(rng.bounded(1, 48));
            for (int i = 0; i < hayLen; ++i) hay[i] = std::uint8_t(rng.bounded(0, 4));   // tiny alphabet -> many near matches
            for (int i = 0; i < needleLen; ++i) needle[i] = std::uint8_t(rng.bounded(0, 4));
            const std::int64_t got = memSearch(hay, std::size_t(hayLen), needle, std::size_t(needleLen));
            const std::int64_t want = memSearchReference(hay, std::size_t(hayLen), needle, std::size_t(needleLen));
            if (got != want)
                QFAIL(qPrintable(QString("memSearch mismatch at iter %1: %2 vs %3").arg(iter).arg(got).arg(want)));
        }
    }

    void sha256NistVector()
    {
        // FIPS 180-4 "abc" -> ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
        const QByteArray digest = sha256ViaAsmCore("abc");
        QCOMPARE(digest.toHex(),
                 QByteArray("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
        // Two-block message from the FIPS appendix.
        QCOMPARE(sha256ViaAsmCore("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq").toHex(),
                 QByteArray("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
    }

    void sha256MatchesQt()
    {
        // Full-pipeline differential vs Qt's SHA-256 across lengths that
        // exercise single-block, multi-block and padding boundary paths.
        QRandomGenerator rng(777u);
        QByteArray msg;
        msg.reserve(300);
        for (int len = 0; len <= 200; ++len) {
            msg.resize(len);
            for (int i = 0; i < len; ++i) msg[i] = char(rng.bounded(0, 256));
            const QByteArray ours = sha256ViaAsmCore(msg);
            const QByteArray qt = QCryptographicHash::hash(msg, QCryptographicHash::Sha256);
            if (ours != qt)
                QFAIL(qPrintable(QString("sha256 mismatch at len %1: %2 vs %3")
                                     .arg(len).arg(QString(ours.toHex())).arg(QString(qt.toHex()))));
        }
        QCOMPARE(sha256ViaAsmCore(QByteArray(64, 'x')).size(), 32);
    }
};

QTEST_GUILESS_MAIN(TestAsmCore)
#include "test_asmcore.moc"
