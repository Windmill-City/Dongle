#ifndef RF_UART_RX_H
#define RF_UART_RX_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <ch572rf.h>
#include "ch57x_common.h"
#include "buf.h"

#define RF_BUF_LEN       512

#define CONN_INTERVAL    10
#define CONN_TIMEOUT     100
#define CONN_PHY_TYPE    1

void rf_uart_rx_init(void);
uint8_t rf_uart_rx_read(void *buf, buf_size_t *len);

#ifdef __cplusplus
}
#endif

#endif
