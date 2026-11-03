#ifndef __RF_UART_RX_H
#define __RF_UART_RX_H

#ifdef __cplusplus
extern "C"
{
#endif
#include <CH572rf.h>
#include "CH57x_common.h"
#include "buf.h"

#define   RF_BUF_LEN   512

#define  CONN_INTERVAL     10
#define  CONN_TIMEOUT      100
#define  CONN_PHY_TYPE     1

void RF_UartRxInit( void );
uint8_t RF_RxQuery( void *buf, typeBufSize *len );

#ifdef __cplusplus
}
#endif

#endif
