#pragma once

#include <cstddef>
#include <cstdint>

#include "ch572/compiler.hpp"
#include "ch572/platform/registers.hpp"

namespace ch572 {

// The WCH core has a byte-oriented block copy instruction.  It is used here so
// the interrupt-side ring buffer does not need to call into libc.
inline void wch_mcpy(void *dst, const void *src, std::size_t length) noexcept
{
    auto *d = static_cast<std::uint8_t *>(dst);
    auto *s = const_cast<std::uint8_t *>(static_cast<const std::uint8_t *>(src));
    auto *end = s + length;
    asm volatile("mcpy %2, %0, %1" : "+r"(s), "+r"(d) : "r"(end) : "memory");
}

template <std::size_t N>
class RingBuffer
{
public:
    static_assert(N > 0, "RingBuffer must have storage");

    std::size_t size() const noexcept
    {
        CriticalSection guard;
        return m_count;
    }

    bool empty() const noexcept { return size() == 0; }

    std::size_t write(const void *src, std::size_t length) noexcept
    {
        if (src == nullptr || length == 0)
        {
            return 0;
        }

        {
            CriticalSection guard;
            if (length > (N - m_count))
            {
                return 0;
            }
        }

        const std::size_t first = N - m_write;
        if (length <= first)
        {
            wch_mcpy(&m_pool[m_write], src, length);
            m_write += length;
            if (m_write == N)
            {
                m_write = 0;
            }
        }
        else
        {
            wch_mcpy(&m_pool[m_write], src, first);
            wch_mcpy(&m_pool[0],
                     static_cast<const std::uint8_t *>(src) + first,
                     length - first);
            m_write = length - first;
        }

        CriticalSection guard;
        m_count += length;
        return length;
    }

    std::size_t read(void *dst, std::size_t length) noexcept
    {
        if (dst == nullptr)
        {
            return 0;
        }

        std::size_t available;
        {
            CriticalSection guard;
            available = m_count;
        }
        if (available == 0 || length == 0)
        {
            return 0;
        }
        if (length > available)
        {
            length = available;
        }

        const std::size_t first = N - m_read;
        if (length <= first)
        {
            wch_mcpy(dst, &m_pool[m_read], length);
            m_read += length;
            if (m_read == N)
            {
                m_read = 0;
            }
        }
        else
        {
            wch_mcpy(dst, &m_pool[m_read], first);
            wch_mcpy(static_cast<std::uint8_t *>(dst) + first,
                     &m_pool[0],
                     length - first);
            m_read = length - first;
        }

        CriticalSection guard;
        m_count -= length;
        return length;
    }

private:
    std::uint8_t m_pool[N]{};
    std::size_t m_write = 0;
    std::size_t m_read = 0;
    volatile std::size_t m_count = 0;
};

} // namespace ch572
