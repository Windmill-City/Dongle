#ifndef BUF_H
#define BUF_H

#include <stdint.h>
#include <stddef.h>

typedef unsigned long buf_size_t;

struct byte_buf {
    uint8_t *start;
    uint8_t *end;
    uint8_t *read;
    uint8_t *write;
    uint16_t buf_len;
    uint16_t volatile data_len;
};

struct byte_buf *byte_buf_create(struct byte_buf *buf,
            uint8_t *buf_pool, uint16_t pool_size);

buf_size_t byte_buf_write(struct byte_buf *buf, void *src, buf_size_t *len);

buf_size_t byte_buf_read(struct byte_buf *buf, void *dst, buf_size_t *len);

#endif
