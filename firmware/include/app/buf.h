#ifndef BUF_H
#define BUF_H

#include <stdint.h>
#include <stddef.h>

#ifndef typeBufSize
typedef unsigned long   typeBufSize;
#endif

struct simple_buf {
    uint8_t *start;
    uint8_t *end;
    uint8_t *read;
    uint8_t *write;
    uint16_t  buf_len;
    uint16_t volatile data_len;
};

struct simple_buf *simple_buf_create(struct simple_buf *buf,
            uint8_t *buf_pool, uint16_t pool_size);

typeBufSize write_buf(struct simple_buf *buf, void *src, typeBufSize *len );

typeBufSize read_buf( struct simple_buf *buf, void *dst, typeBufSize *len );

#endif
