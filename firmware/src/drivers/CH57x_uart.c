#include "CH57x_common.h"

void UART_DefInit(void)
{
    UART_BaudRateCfg(115200);
    R8_UART_FCR = (2 << 6) | RB_FCR_FIFO_EN;
    R8_UART_LCR = RB_LCR_WORD_SZ;
    R8_UART_IER = RB_IER_TXD_EN;
    R8_UART_DIV = 1;
}

void UART_BaudRateCfg(uint32_t baudrate)
{
    uint32_t x;

    x = 10 * GetSysClock() / 8 / baudrate;
    x = (x + 5) / 10;
    R16_UART_DL = (uint16_t)x;
}

void UART_ByteTrigCfg(UARTByteTRIGTypeDef b)
{
    R8_UART_FCR = (R8_UART_FCR & ~RB_FCR_FIFO_TRIG) | (b << 6);
}

void UART_INTCfg(FunctionalState s, uint8_t i)
{
    if(s)
    {
        R8_UART_IER |= i;
        R8_UART_MCR |= RB_MCR_INT_OE;
    }
    else
    {
        R8_UART_IER &= ~i;
    }
}

void UART_SendString(uint8_t *buf, uint16_t l)
{
    uint16_t len = l;

    while(len)
    {
        if(R8_UART_TFC != UART_FIFO_SIZE)
        {
            R8_UART_THR = *buf++;
            len--;
        }
    }
}

uint16_t UART_RecvString(uint8_t *buf)
{
    uint16_t len = 0;

    while(R8_UART_RFC)
    {
        *buf++ = R8_UART_RBR;
        len++;
    }

    return (len);
}

void UART_Remap(FunctionalState s, UARTTxPinRemapDef u_tx, UARTRxPinRemapDef u_rx)
{
    if(s)
    {
        R16_PIN_ALTERNATE_H &= ~(RB_UART_TXD | RB_UART_RXD);
        R16_PIN_ALTERNATE_H |= (u_tx << 3) | u_rx;
    }
    else
    {
        R16_PIN_ALTERNATE_H &= ~(RB_UART_TXD | RB_UART_RXD);
    }
}
