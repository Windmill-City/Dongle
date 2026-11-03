#ifndef __USB_UART_H
#define __USB_UART_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "CH57x_common.h"
#include "buf.h"

 typedef struct __PACKED _LINE_CODE
{
  uint32_t  BaudRate;
  uint8_t StopBits;
  uint8_t ParityType;
  uint8_t DataBits;
  uint8_t ioStaus;
}LINE_CODE, *PLINE_CODE;
extern LINE_CODE Uart0Para;

void USB_Init( void );
void USB_StatusQuery( void );
uint8_t USB_RxQuery( void *buf, typeBufSize *len );

#ifdef __cplusplus
}
#endif

#endif
