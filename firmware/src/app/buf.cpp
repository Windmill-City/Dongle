#include "buf.hpp"

template <uint16_t N>
__HIGH_CODE buf_size_t ByteBuf<N>::write(const void *src, buf_size_t len)
{
    uint16_t free_len;
    uint16_t tmp;
    const uint8_t *s = static_cast<const uint8_t *>(src);

    PFIC_DisableAllIRQ();
    free_len = N - m_count;
    PFIC_EnableAllIRQ();

    if (len > free_len)
    {
        return 0;
    }

    tmp = N - m_wr;
    if (len <= tmp)
    {
        __MCPY(&m_pool[m_wr], const_cast<uint8_t *>(s),
               const_cast<uint8_t *>(s + len));
        m_wr += len;
        if (m_wr == N) m_wr = 0;
    }
    else
    {
        __MCPY(&m_pool[m_wr], const_cast<uint8_t *>(s),
               const_cast<uint8_t *>(s + tmp));
        __MCPY(&m_pool[0], const_cast<uint8_t *>(s + tmp),
               const_cast<uint8_t *>(s + len));
        m_wr = len - tmp;
    }

    PFIC_DisableAllIRQ();
    m_count += len;
    PFIC_EnableAllIRQ();
    return len;
}

template <uint16_t N>
__HIGH_CODE buf_size_t ByteBuf<N>::read(void *dst, buf_size_t len)
{
    uint16_t avail;
    uint16_t tmp;
    uint8_t *d = static_cast<uint8_t *>(dst);

    PFIC_DisableAllIRQ();
    avail = m_count;
    PFIC_EnableAllIRQ();

    if (avail == 0)
    {
        return 0;
    }
    if (len > avail)
    {
        len = avail;
    }

    tmp = N - m_rd;
    if (len <= tmp)
    {
        __MCPY(d, &m_pool[m_rd], &m_pool[m_rd] + len);
        m_rd += len;
        if (m_rd == N) m_rd = 0;
    }
    else
    {
        __MCPY(d, &m_pool[m_rd], &m_pool[N]);
        __MCPY(d + tmp, &m_pool[0], &m_pool[len - tmp]);
        m_rd = len - tmp;
    }

    PFIC_DisableAllIRQ();
    m_count -= len;
    PFIC_EnableAllIRQ();
    return len;
}

template class ByteBuf<256>;
template class ByteBuf<512>;
