#include "CH57x_common.h"

void SPI_MasterDefInit(void)
{
    R8_SPI_CLOCK_DIV = 4;
    R8_SPI_CTRL_MOD = RB_SPI_ALL_CLEAR;
    R8_SPI_CTRL_MOD = RB_SPI_MOSI_OE | RB_SPI_SCK_OE;
    R8_SPI_CTRL_CFG |= RB_SPI_AUTO_IF;
    R8_SPI_CTRL_CFG &= ~RB_SPI_DMA_ENABLE;
}

void SPI_2WIRE_MasterOutputInit(void)
{
    R8_SPI_CLOCK_DIV = 4;
    R8_SPI_CTRL_MOD = RB_SPI_ALL_CLEAR;
    R8_SPI_CTRL_MOD =  RB_SPI_MOSI_OE | RB_SPI_SCK_OE | RB_SPI_2WIRE_MOD;
    R8_SPI_CTRL_CFG |= RB_SPI_AUTO_IF;
    R8_SPI_CTRL_CFG &= ~RB_SPI_DMA_ENABLE;
}

void SPI_2WIRE_MasterReceiveInit(void)
{
    R8_SPI_CLOCK_DIV = 4;
    R8_SPI_CTRL_MOD = RB_SPI_ALL_CLEAR;
    R8_SPI_CTRL_MOD = RB_SPI_SCK_OE | RB_SPI_2WIRE_MOD;
    R8_SPI_CTRL_CFG |= RB_SPI_AUTO_IF;
    R8_SPI_CTRL_CFG &= ~RB_SPI_DMA_ENABLE;
}

void SPI_2WIRE_SlaveInputInit(void)
{
    R8_SPI_CTRL_MOD =  RB_SPI_ALL_CLEAR;
    R8_SPI_CTRL_MOD =  RB_SPI_2WIRE_MOD | RB_SPI_MODE_SLAVE;
    R8_SPI_CTRL_CFG |= RB_SPI_AUTO_IF | RB_MST_CLK_SEL;
}

void SPI_2WIRE_SlaveOutputInit(void)
{
    R8_SPI_CTRL_MOD =  RB_SPI_ALL_CLEAR;
    R8_SPI_CTRL_MOD =  RB_SPI_MISO_OE | RB_SPI_2WIRE_MOD | RB_SPI_MODE_SLAVE;
    R8_SPI_CTRL_CFG |= RB_SPI_AUTO_IF;
}

void SPI_CLKCfg(uint8_t c)
{
    if(c == 2)
    {
        R8_SPI_CTRL_CFG |= RB_SPI_MST_DLY_EN;
    }
    else
    {
        R8_SPI_CTRL_CFG &= ~RB_SPI_MST_DLY_EN;
    }
    R8_SPI_CLOCK_DIV = c;
}

void SPI_DataMode(ModeBitOrderTypeDef m)
{
    switch(m)
    {
        case Mode0_LowBitINFront:
            R8_SPI_CTRL_MOD &= ~RB_SPI_MST_SCK_MOD;
            R8_SPI_CTRL_CFG |= RB_SPI_BIT_ORDER;
            break;
        case Mode0_HighBitINFront:
            R8_SPI_CTRL_MOD &= ~RB_SPI_MST_SCK_MOD;
            R8_SPI_CTRL_CFG &= ~RB_SPI_BIT_ORDER;
            break;
        case Mode3_LowBitINFront:
            R8_SPI_CTRL_MOD |= RB_SPI_MST_SCK_MOD;
            R8_SPI_CTRL_CFG |= RB_SPI_BIT_ORDER;
            break;
        case Mode3_HighBitINFront:
            R8_SPI_CTRL_MOD |= RB_SPI_MST_SCK_MOD;
            R8_SPI_CTRL_CFG &= ~RB_SPI_BIT_ORDER;
            break;
        default:
            break;
    }
}

void SPI_MasterSendByte(uint8_t d)
{
    R8_SPI_CTRL_MOD &= ~RB_SPI_FIFO_DIR;
    R16_SPI_TOTAL_CNT = 1;
    R8_SPI_FIFO = d;
    while(!(R8_SPI_INT_FLAG & RB_SPI_FREE));
}

uint8_t SPI_MasterRecvByte(void)
{
    R8_SPI_CTRL_MOD &= ~RB_SPI_FIFO_DIR;
    R8_SPI_BUFFER = 0xFF;
    while(!(R8_SPI_INT_FLAG & RB_SPI_FREE));
    return (R8_SPI_BUFFER);
}

void SPI_MasterTrans(uint8_t *pbuf, uint16_t len)
{
    uint16_t sendlen;

    sendlen = len;
    R8_SPI_CTRL_MOD &= ~RB_SPI_FIFO_DIR;
    R16_SPI_TOTAL_CNT = sendlen;
    R8_SPI_INT_FLAG = RB_SPI_IF_CNT_END;
    while(sendlen)
    {
        if(R8_SPI_FIFO_COUNT < SPI_FIFO_SIZE)
        {
            R8_SPI_FIFO = *pbuf;
            pbuf++;
            sendlen--;
        }
    }
    while(R8_SPI_FIFO_COUNT != 0);
}

void SPI_MasterRecv(uint8_t *pbuf, uint16_t len)
{
    uint16_t readlen;

    readlen = len;
    R8_SPI_CTRL_MOD |= RB_SPI_FIFO_DIR;
    R16_SPI_TOTAL_CNT = len;
    R8_SPI_INT_FLAG = RB_SPI_IF_CNT_END;
    while(readlen)
    {
        if(R8_SPI_FIFO_COUNT)
        {
            *pbuf = R8_SPI_FIFO;
            pbuf++;
            readlen--;
        }
    }
}

void SPI_MasterDMATrans(uint8_t *pbuf, uint16_t len)
{
    R8_SPI_CTRL_MOD &= ~RB_SPI_FIFO_DIR;
    R16_SPI_DMA_BEG = (uint32_t)pbuf;
    R16_SPI_DMA_END = (uint32_t)(pbuf + len);
    R16_SPI_TOTAL_CNT = len;
    R8_SPI_INT_FLAG = RB_SPI_IF_CNT_END | RB_SPI_IF_DMA_END;
    R8_SPI_CTRL_CFG |= RB_SPI_DMA_ENABLE;
    while(!(R8_SPI_INT_FLAG & RB_SPI_IF_CNT_END));
    R8_SPI_CTRL_CFG &= ~RB_SPI_DMA_ENABLE;
}

void SPI_MasterDMARecv(uint8_t *pbuf, uint16_t len)
{
    R8_SPI_CTRL_MOD |= RB_SPI_FIFO_DIR;
    R16_SPI_DMA_BEG = (uint32_t)pbuf;
    R16_SPI_DMA_END = (uint32_t)(pbuf + len);
    R16_SPI_TOTAL_CNT = len;
    R8_SPI_INT_FLAG = RB_SPI_IF_CNT_END | RB_SPI_IF_DMA_END;
    R8_SPI_CTRL_CFG |= RB_SPI_DMA_ENABLE;
    while(!(R8_SPI_INT_FLAG & RB_SPI_IF_CNT_END));
    R8_SPI_CTRL_CFG &= ~RB_SPI_DMA_ENABLE;
}

void SPI_SlaveInit(void)
{
    R8_SPI_CTRL_MOD = RB_SPI_ALL_CLEAR;
    R8_SPI_CTRL_MOD = RB_SPI_MISO_OE | RB_SPI_MODE_SLAVE;
    R8_SPI_CTRL_CFG |= RB_SPI_AUTO_IF;
}

void SPI_2WIRE_SlaveInit(void)
{
    R8_SPI_CTRL_MOD = RB_SPI_ALL_CLEAR;
    R8_SPI_CTRL_MOD = RB_SPI_MISO_OE | RB_SPI_2WIRE_MOD | RB_SPI_MODE_SLAVE;
    R8_SPI_CTRL_CFG |= RB_SPI_AUTO_IF;
}

uint8_t SPI_SlaveRecvByte(void)
{
    R8_SPI_CTRL_MOD |= RB_SPI_FIFO_DIR;
    while(R8_SPI_FIFO_COUNT == 0);
    return R8_SPI_FIFO;
}

void SPI_SlaveSendByte(uint8_t d)
{
    R8_SPI_CTRL_MOD &= ~RB_SPI_FIFO_DIR;
    R16_SPI_TOTAL_CNT = 1;
    R8_SPI_FIFO = d;
    while(R8_SPI_FIFO_COUNT != 0);
}

__HIGH_CODE
void SPI_SlaveRecv(uint8_t *pbuf, uint16_t len)
{
    uint16_t revlen;

    revlen = len;
    R8_SPI_CTRL_MOD |= RB_SPI_FIFO_DIR;
    R8_SPI_INT_FLAG = RB_SPI_IF_CNT_END;
    while(revlen)
    {
        if(R8_SPI_FIFO_COUNT)
        {
            *pbuf = R8_SPI_FIFO;
            pbuf++;
            revlen--;
        }
    }
}

__HIGH_CODE
void SPI_SlaveTrans(uint8_t *pbuf, uint16_t len)
{
    uint16_t sendlen;

    sendlen = len;
    R8_SPI_CTRL_MOD &= ~RB_SPI_FIFO_DIR;
    R8_SPI_INT_FLAG = RB_SPI_IF_CNT_END;
    while(sendlen)
    {
        if(R8_SPI_FIFO_COUNT < SPI_FIFO_SIZE)
        {
            R8_SPI_FIFO = *pbuf;
            pbuf++;
            sendlen--;
        }
    }
    while(R8_SPI_FIFO_COUNT != 0);
}

void SPI_SlaveDMARecv(uint8_t *pbuf, uint16_t len)
{
    R8_SPI_CTRL_MOD |= RB_SPI_FIFO_DIR;
    R16_SPI_DMA_BEG = (uint32_t)pbuf;
    R16_SPI_DMA_END = (uint32_t)(pbuf + len);
    R16_SPI_TOTAL_CNT = len;
    R8_SPI_INT_FLAG = RB_SPI_IF_CNT_END | RB_SPI_IF_DMA_END;
    R8_SPI_CTRL_CFG |= RB_SPI_DMA_ENABLE;
    while(!(R8_SPI_INT_FLAG & RB_SPI_IF_CNT_END));
    R8_SPI_CTRL_CFG &= ~RB_SPI_DMA_ENABLE;
}

void SPI_SlaveDMATrans(uint8_t *pbuf, uint16_t len)
{
    R8_SPI_CTRL_MOD &= ~RB_SPI_FIFO_DIR;
    R16_SPI_DMA_BEG = (uint32_t)pbuf;
    R16_SPI_DMA_END = (uint32_t)(pbuf + len);
    R16_SPI_TOTAL_CNT = len;
    R8_SPI_INT_FLAG = RB_SPI_IF_CNT_END | RB_SPI_IF_DMA_END;
    R8_SPI_CTRL_CFG |= RB_SPI_DMA_ENABLE;
    while(!(R8_SPI_INT_FLAG & RB_SPI_IF_CNT_END));
    R8_SPI_CTRL_CFG &= ~RB_SPI_DMA_ENABLE;
}
