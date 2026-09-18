#pragma once
// ============================================================================
// CodeForge Assembly Core (v1.3.0)
// ============================================================================
// Real x86-64 assembly (MASM .asm files in this directory) compiled into the
// Windows binary, with bit-exact portable C++ fallbacks for Linux/macOS
// builds. Every function is differentially tested in tests/test_asmcore.cpp:
// on Windows CI the tests execute the .asm machine code; on Linux/macOS CI
// they execute the fallbacks, so semantics are pinned on every platform.
//
// Honest scope note: this is an Assembly core for hot-path primitives, not a
// 100%-assembly application. The UI framework (Qt) and the remaining modules
// stay C++; a full hand-written-assembly editor is not a maintainable or
// realistic deliverable (see README, "Assembly Core" section).
// ============================================================================
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

#if defined(_WIN32) && defined(_MSC_VER) && defined(_M_X64)
#define CF_ASM_CORE 1
extern "C" {
// implementations in src/asm/*.asm (MASM/ml64)
void         cf_asm_secure_zero(void* p, std::size_t n);
std::uint32_t cf_asm_crc32c(std::uint32_t seed, const void* p, std::size_t n);
int          cf_asm_utf8_validate(const char* s, std::size_t n);
std::int64_t cf_asm_memsearch(const std::uint8_t* hay, std::size_t hayLen,
                              const std::uint8_t* needle, std::size_t needleLen);
void         cf_asm_sha256_compress(std::uint32_t state[8], const std::uint8_t block[64]);
}
#endif

namespace cf::asmcore {

// ---------------------------------------------------------------------------
// secureZero: non-elidable memory wipe (security layer hot path).
// ---------------------------------------------------------------------------
inline void secureZero(void* p, std::size_t n)
{
#if CF_ASM_CORE
    if (n) cf_asm_secure_zero(p, n);
#else
    volatile std::uint8_t* v = static_cast<volatile std::uint8_t*>(p);
    for (std::size_t i = 0; i < n; ++i) v[i] = 0;
#endif
}

// ---------------------------------------------------------------------------
// crc32c: CRC-32C (Castagnoli). seed=0 gives published check values;
// chaining: crc32c(crc32c(0,a), b) == crc32c(0, a+b).
// ---------------------------------------------------------------------------
inline std::uint32_t crc32c(std::uint32_t seed, const void* p, std::size_t n)
{
#if CF_ASM_CORE
    return cf_asm_crc32c(seed, p, n);
#else
    auto* bytes = static_cast<const std::uint8_t*>(p);
    std::uint32_t crc = seed ^ 0xFFFFFFFFu;
    for (std::size_t i = 0; i < n; ++i) {
        crc ^= bytes[i];
        for (int k = 0; k < 8; ++k)
            crc = (crc >> 1) ^ (0x82F63B78u & (0u - (crc & 1u)));
    }
    return crc ^ 0xFFFFFFFFu;
#endif
}

// ---------------------------------------------------------------------------
// utf8Valid: strict UTF-8 validation (rejects overlongs, surrogates,
// > U+10FFFF, truncated and lone-continuation sequences).
// ---------------------------------------------------------------------------
inline bool utf8Valid(const char* s, std::size_t n)
{
#if CF_ASM_CORE
    return cf_asm_utf8_validate(s, n) != 0;
#else
    const auto* b = reinterpret_cast<const std::uint8_t*>(s);
    std::size_t i = 0;
    auto cont = [&](std::uint8_t lo, std::uint8_t hi) -> bool {
        if (i >= n || b[i] < lo || b[i] > hi) return false;
        ++i; return true;
    };
    while (i < n) {
        std::uint8_t c = b[i];
        ++i;
        if (c < 0x80) continue;
        if (c >= 0xC2 && c <= 0xDF) { if (!cont(0x80, 0xBF)) return false; continue; }
        if (c == 0xE0) { if (!cont(0xA0, 0xBF) || !cont(0x80, 0xBF)) return false; continue; }
        if ((c >= 0xE1 && c <= 0xEC) || c == 0xEE || c == 0xEF) {
            if (!cont(0x80, 0xBF) || !cont(0x80, 0xBF)) return false; continue; }
        if (c == 0xED) { if (!cont(0x80, 0x9F) || !cont(0x80, 0xBF)) return false; continue; }
        if (c == 0xF0) { if (!cont(0x90, 0xBF) || !cont(0x80, 0xBF) || !cont(0x80, 0xBF)) return false; continue; }
        if (c >= 0xF1 && c <= 0xF3) {
            if (!cont(0x80, 0xBF) || !cont(0x80, 0xBF) || !cont(0x80, 0xBF)) return false; continue; }
        if (c == 0xF4) { if (!cont(0x80, 0x8F) || !cont(0x80, 0xBF) || !cont(0x80, 0xBF)) return false; continue; }
        return false; // 0x80..0xC1 lone/overlong, 0xF5..0xFF out of range
    }
    return true;
#endif
}
inline bool utf8Valid(std::string_view s) { return utf8Valid(s.data(), s.size()); }

// ---------------------------------------------------------------------------
// memSearch: first occurrence of needle in hay; empty needle -> 0,
// not found -> -1.
// ---------------------------------------------------------------------------
inline std::int64_t memSearch(const std::uint8_t* hay, std::size_t hayLen,
                              const std::uint8_t* needle, std::size_t needleLen)
{
#if CF_ASM_CORE
    return cf_asm_memsearch(hay, hayLen, needle, needleLen);
#else
    if (needleLen == 0) return 0;
    if (needleLen > hayLen) return -1;
    const std::size_t last = hayLen - needleLen;
    for (std::size_t i = 0; i <= last; ++i) {
        if (hay[i] != needle[0]) continue;
        std::size_t j = 1;
        while (j < needleLen && hay[i + j] == needle[j]) ++j;
        if (j == needleLen) return static_cast<std::int64_t>(i);
    }
    return -1;
#endif
}

// ---------------------------------------------------------------------------
// sha256Compress: FIPS 180-4 compression of one 64-byte block. Chaining is
// the caller's job: start from the IV below, one call per padded block.
// ---------------------------------------------------------------------------
struct Sha256IV { static constexpr std::uint32_t words[8] = {
    0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
    0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u }; };

inline std::uint32_t rotr32(std::uint32_t x, unsigned r)
{ return (x >> r) | (x << (32u - r)); }

inline void sha256Compress(std::uint32_t state[8], const std::uint8_t block[64])
{
#if CF_ASM_CORE
    cf_asm_sha256_compress(state, block);
#else
    std::uint32_t w[64];
    for (int i = 0; i < 16; ++i)
        w[i] = (std::uint32_t(block[4*i]) << 24) | (std::uint32_t(block[4*i+1]) << 16)
             | (std::uint32_t(block[4*i+2]) << 8) | std::uint32_t(block[4*i+3]);
    for (int i = 16; i < 64; ++i) {
        std::uint32_t s0 = rotr32(w[i-15], 7) ^ rotr32(w[i-15], 18) ^ (w[i-15] >> 3);
        std::uint32_t s1 = rotr32(w[i-2], 17) ^ rotr32(w[i-2], 19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    std::uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    std::uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
    for (int i = 0; i < 64; ++i) {
        const std::uint32_t S1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
        const std::uint32_t ch = (e & f) ^ (~e & g);
        static constexpr std::uint32_t KK[64] = {
            0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
            0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
            0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
            0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
            0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
            0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
            0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
            0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u };
        const std::uint32_t t1 = h + S1 + ch + KK[i] + w[i];
        const std::uint32_t S0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
        const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t t2 = S0 + maj;
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
#endif
}

}  // namespace cf::asmcore
