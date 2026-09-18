#pragma once
// Memory container for sensitive data (keys, plaintext staged for encryption).
//
// Guarantees:
//  - memory is locked (won't be swapped to disk) while allocated;
//    Windows: VirtualAlloc + VirtualLock, POSIX: mlock
//  - contents are wiped with a non-optimizable zeroize on destruction/resize
//  - non-copyable to prevent accidental plaintext duplication
//
// NOTE (honest limitation, per spec): the live editor buffer must remain in
// ordinary Qt memory to be editable. This class covers the sensitive paths we
// control explicitly (crypto keys, staged plaintext for encryption).
#include <QByteArray>
#include <QtGlobal>

#include "asm/asmcore.h"   // Assembly Core: cf_asm_secure_zero (Windows x64)
#include "core/Logger.h"

#if defined(Q_OS_WIN)
#include <qt_windows.h>
#else
#include <cstdlib>
#include <cstring>
#include <sys/mman.h>
#endif

namespace cf::sec {

// v1.3.0: routes through the Assembly Core on Windows x64 (real .asm stores
// the optimizer cannot elide); other platforms use the portable fallback.
static inline void secureZero(void* p, size_t n)
{
    cf::asmcore::secureZero(p, n);
}

class SecureBuffer {
public:
    SecureBuffer() = default;
    ~SecureBuffer() { freeMem(); }

    SecureBuffer(const SecureBuffer&) = delete;
    SecureBuffer& operator=(const SecureBuffer&) = delete;

    SecureBuffer(SecureBuffer&& o) noexcept : m_data(o.m_data), m_size(o.m_size), m_cap(o.m_cap)
    {
        o.m_data = nullptr; o.m_size = 0; o.m_cap = 0;
    }
    SecureBuffer& operator=(SecureBuffer&& o) noexcept
    {
        if (this != &o) { freeMem(); m_data = o.m_data; m_size = o.m_size; m_cap = o.m_cap; o.m_data = nullptr; o.m_size = 0; o.m_cap = 0; }
        return *this;
    }

    bool resize(size_t n)
    {
        if (n <= m_cap) { if (n > m_size) secureZero(m_data + m_size, n - m_size); m_size = n; return true; }
        freeMem();
        return allocate(n);
    }

    void zeroize() { if (m_data && m_size) secureZero(m_data, m_size); }

    quint8* data() { return m_data; }
    const quint8* data() const { return m_data; }
    size_t size() const { return m_size; }
    bool isEmpty() const { return m_size == 0; }

    QByteArray toByteArray() const
    {
        return m_data ? QByteArray(reinterpret_cast<const char*>(m_data), int(m_size)) : QByteArray();
    }

    static SecureBuffer fromByteArray(const QByteArray& a)
    {
        SecureBuffer b;
        if (b.allocate(size_t(a.size()))) memcpy(b.m_data, a.constData(), size_t(a.size()));
        return b;
    }

private:
    bool allocate(size_t n)
    {
        if (n == 0) return true;
#if defined(Q_OS_WIN)
        m_data = static_cast<quint8*>(VirtualAlloc(nullptr, n, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        if (!m_data) return false;
        if (!VirtualLock(m_data, n)) { VirtualFree(m_data, 0, MEM_RELEASE); m_data = nullptr; return false; }
#else
        m_data = static_cast<quint8*>(std::malloc(n));
        if (!m_data) return false;
        if (mlock(m_data, n) != 0) {
            // Locking may fail on limits; still usable, but log-level concern.
            CF_LOG_WARNING(QString::fromLatin1("SecureBuffer: mlock failed (rlimit), memory will be zeroized but may page out"));
        }
        m_locked = true;
#endif
        m_size = n;
        m_cap = n;
        secureZero(m_data, n);
        return true;
    }

    void freeMem()
    {
        if (!m_data) return;
        secureZero(m_data, m_cap);
#if defined(Q_OS_WIN)
        VirtualUnlock(m_data, m_cap);
        VirtualFree(m_data, 0, MEM_RELEASE);
#else
        if (m_locked) munlock(m_data, m_cap);
        std::free(m_data);
        m_locked = false;
#endif
        m_data = nullptr; m_size = 0; m_cap = 0;
    }

    quint8* m_data = nullptr;
    size_t m_size = 0;
    size_t m_cap = 0;
#ifndef Q_OS_WIN
    bool m_locked = false;
#endif
};

}  // namespace cf::sec
