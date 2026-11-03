#ifndef BUF_HPP
#define BUF_HPP

#include <stdint.h>
#include "common.hpp"

typedef unsigned long buf_size_t;

template <uint16_t N>
class ByteBuf
{
public:
    static constexpr uint16_t capacity = N;

    uint16_t size() const { return m_count; }
    bool empty() const { return m_count == 0; }
    uint16_t free_size() const { return N - m_count; }

    __HIGH_CODE buf_size_t write(const void *src, buf_size_t len);
    __HIGH_CODE buf_size_t read(void *dst, buf_size_t len);

private:
    uint8_t m_pool[N];
    uint16_t m_wr;
    uint16_t m_rd;
    volatile uint16_t m_count;
};

#endif
