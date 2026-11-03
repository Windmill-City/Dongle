#ifndef __CH57x_SPI_H__
#define __CH57x_SPI_H__

#ifdef __cplusplus
extern "C" {
#endif

#define SPI_IT_FST_BYTE    RB_SPI_IE_FST_BYTE
#define SPI_IT_FIFO_OV     RB_SPI_IE_FIFO_OV
#define SPI_IT_DMA_END     RB_SPI_IE_DMA_END
#define SPI_IT_FIFO_HF     RB_SPI_IE_FIFO_HF
#define SPI_IT_BYTE_END    RB_SPI_IE_BYTE_END
#define SPI_IT_CNT_END     RB_SPI_IE_CNT_END

typedef enum
{
    Mode0_LowBitINFront = 0,
    Mode0_HighBitINFront,
    Mode3_LowBitINFront,
    Mode3_HighBitINFront,
} ModeBitOrderTypeDef;

typedef enum
{
    Mode_DataStream = 0,
    Mose_FirstCmd,
} Slave_ModeTypeDef;

void SPI_MasterDefInit(void);

void SPI_2WIRE_MasterOutputInit(void);

void SPI_2WIRE_MasterReceiveInit(void);

void SPI_2WIRE_SlaveInputInit(void);

void SPI_2WIRE_SlaveOutputInit(void);

void SPI_CLKCfg(uint8_t c);

void SPI_DataMode(ModeBitOrderTypeDef m);

void SPI_MasterSendByte(uint8_t d);

uint8_t SPI_MasterRecvByte(void);

void SPI_MasterTrans(uint8_t *pbuf, uint16_t len);

void SPI_MasterRecv(uint8_t *pbuf, uint16_t len);

void SPI_MasterDMATrans(uint8_t *pbuf, uint16_t len);

void SPI_MasterDMARecv(uint8_t *pbuf, uint16_t len);

#define SetFirstData(d)    (R8_SPI_SLAVE_PRE = d)

void SPI_SlaveInit(void);

void SPI_2WIRE_SlaveInit(void);

void SPI_SlaveSendByte(uint8_t d);

uint8_t SPI_SlaveRecvByte(void);

void SPI_SlaveTrans(uint8_t *pbuf, uint16_t len);

void SPI_SlaveRecv(uint8_t *pbuf, uint16_t len);

void SPI_SlaveDMATrans(uint8_t *pbuf, uint16_t len);

void SPI_SlaveDMARecv(uint8_t *pbuf, uint16_t len);

#define SPI_ITCfg(s, f)       ((s) ? (R8_SPI_INTER_EN |= f) : (R8_SPI_INTER_EN &= ~f))

#define SPI_GetITFlag(f)      (R8_SPI_INT_FLAG & f)

#define SPI_ClearITFlag(f)    (R8_SPI_INT_FLAG = f)

#define SPI_Disable()         (R8_SPI_CTRL_MOD &= ~(RB_SPI_MOSI_OE | RB_SPI_SCK_OE | RB_SPI_MISO_OE))

#ifdef __cplusplus
}
#endif

#endif
