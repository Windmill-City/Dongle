#ifndef USB_UART_H
#define USB_UART_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "ch57x_common.h"
#include "buf.h"

typedef struct __attribute__((packed))
{
    uint32_t baud_rate;
    uint8_t stop_bits;
    uint8_t parity_type;
    uint8_t data_bits;
    uint8_t io_status;
} line_code_t;

extern line_code_t uart0_para;

void usb_uart_init(void);
void usb_uart_poll(void);
uint8_t usb_uart_read(void *buf, buf_size_t *len);

#ifdef __cplusplus
}
#endif

#endif
