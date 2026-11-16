#pragma once

#include <cstddef>
#include <cstdint>

namespace ch572
{

inline void wch_mcpy(void* dst, const void* src, std::size_t length) noexcept
{
    auto* d   = static_cast<std::uint8_t*>(dst);
    auto* s   = const_cast<std::uint8_t*>(static_cast<const std::uint8_t*>(src));
    auto* end = s + length;
    asm volatile("mcpy %2, %0, %1" : "+r"(s), "+r"(d) : "r"(end) : "memory");
}

template <std::size_t N>
class RingBuffer
{
  public:
    static_assert(N > 0, "RingBuffer must have storage");

    static constexpr std::size_t capacity = N;

    std::size_t size() const noexcept
    {
        return used();
    }

    bool empty() const noexcept
    {
        return m_read == m_write;
    }

    bool full() const noexcept
    {
        return next(m_write) == m_read;
    }

    std::size_t write(const void* src, std::size_t length) noexcept
    {
        if (src == nullptr || length == 0)
        {
            return 0;
        }

        if (length > capacity - used())
        {
            return 0;
        }

        std::size_t head = m_write;

        const std::size_t first = kSlots - head;
        if (length <= first)
        {
            wch_mcpy(&m_pool[head], src, length);
            head += length;
            if (head == kSlots)
            {
                head = 0;
            }
        }
        else
        {
            wch_mcpy(&m_pool[head], src, first);
            wch_mcpy(&m_pool[0], static_cast<const std::uint8_t*>(src) + first, length - first);
            head = length - first;
        }

        m_write = head;
        return length;
    }

    std::size_t read(void* dst, std::size_t length) noexcept
    {
        if (dst == nullptr || length == 0)
        {
            return 0;
        }

        std::size_t       tail      = m_read;
        const std::size_t available = used();
        if (available == 0)
        {
            return 0;
        }
        if (length > available)
        {
            length = available;
        }

        const std::size_t first = kSlots - tail;
        if (length <= first)
        {
            wch_mcpy(dst, &m_pool[tail], length);
            tail += length;
            if (tail == kSlots)
            {
                tail = 0;
            }
        }
        else
        {
            wch_mcpy(dst, &m_pool[tail], first);
            wch_mcpy(static_cast<std::uint8_t*>(dst) + first, &m_pool[0], length - first);
            tail = length - first;
        }

        m_read = tail;
        return length;
    }

  private:
    static constexpr std::size_t kSlots = N + 1;

    static constexpr std::size_t next(std::size_t index) noexcept
    {
        return index + 1 == kSlots ? 0 : index + 1;
    }

    std::size_t used() const noexcept
    {
        const std::size_t head = m_write;
        const std::size_t tail = m_read;
        return head >= tail ? head - tail : (kSlots - tail) + head;
    }

    std::uint8_t         m_pool[kSlots]{};
    volatile std::size_t m_write = 0;
    volatile std::size_t m_read  = 0;
};

}
