#ifndef __CH57x_FLASH_H__
#define __CH57x_FLASH_H__

#ifdef __cplusplus
extern "C" {
#endif

void FLASH_ROM_READ(uint32_t StartAddr, void *Buffer, uint32_t len);

uint8_t UserOptionByteConfig(FunctionalState RESET_EN, FunctionalState UART_NO_KEY_EN,
        uint32_t FLASHProt_Size);

uint8_t UserOptionByteClose_SWD(void);

void UserOptionByte_Active(void);

void GET_UNIQUE_ID(uint8_t *Buffer);
#ifdef __cplusplus
}
#endif

#endif
