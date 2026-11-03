#ifndef __CH57x_UART_H__
#define __CH57x_UART_H__

#ifdef __cplusplus
extern "C" {
#endif

#define STA_ERR_BREAK     RB_LSR_BREAK_ERR
#define STA_ERR_FRAME     RB_LSR_FRAME_ERR
#define STA_ERR_PAR       RB_LSR_PAR_ERR
#define STA_ERR_FIFOOV    RB_LSR_OVER_ERR

#define STA_TXFIFO_EMP    RB_LSR_TX_FIFO_EMP
#define STA_TXALL_EMP     RB_LSR_TX_ALL_EMP
#define STA_RECV_DATA     RB_LSR_DATA_RDY

typedef enum
{
    UART_1BYTE_TRIG = 0,
    UART_2BYTE_TRIG,
    UART_4BYTE_TRIG,
    UART_7BYTE_TRIG,

} UARTByteTRIGTypeDef;

typedef enum
{
    UART_TX_REMAP_PA3 = 0,
    UART_TX_REMAP_PA2,
    UART_TX_REMAP_PA1,
    UART_TX_REMAP_PA0,
    UART_TX_REMAP_PA7,
    UART_TX_REMAP_PA8,
    UART_TX_REMAP_PA11,
    UART_TX_REMAP_PA10,
} UARTTxPinRemapDef;

typedef enum
{
    UART_RX_REMAP_PA2 = 0,
    UART_RX_REMAP_PA3,
    UART_RX_REMAP_PA0,
    UART_RX_REMAP_PA1,
    UART_RX_REMAP_PA6,
    UART_RX_REMAP_PA9,
    UART_RX_REMAP_PA10,
    UART_RX_REMAP_PA11,
} UARTRxPinRemapDef;

void UART_DefInit(void);

void UART_BaudRateCfg(uint32_t baudrate);

void UART_ByteTrigCfg(UARTByteTRIGTypeDef b);

void UART_INTCfg(FunctionalState s, uint8_t i);

#define UART_CLR_RXFIFO()    (R8_UART_FCR |= RB_FCR_RX_FIFO_CLR)

#define UART_CLR_TXFIFO()    (R8_UART_FCR |= RB_FCR_TX_FIFO_CLR)

#define UART_GetITFlag()     (R8_UART_IIR & RB_IIR_INT_MASK)

#define UART_GetLinSTA()     (R8_UART_LSR)

#define UART_SendByte(b)     (R8_UART_THR = b)

void UART_SendString(uint8_t *buf, uint16_t l);

#define UART_RecvByte()    (R8_UART_RBR)

uint16_t UART_RecvString(uint8_t *buf);

void UART_Remap(FunctionalState s, UARTTxPinRemapDef u_tx, UARTRxPinRemapDef u_rx);

#ifdef __cplusplus
}
#endif

#endif
