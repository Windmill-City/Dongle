#include "CH57x_common.h"

#define RESET_Enable           0x00000008
#define RESET_Disable          0xFFFFFFF7

#define UART_NO_KEY_Enable     0x00000100
#define UART_NO_KEY_Disable    0xFFFFFEFF

#define BOOT_PIN_PB22          0x00000200
#define BOOT_PIN_PB11          0xFFFFFDFF

#define FLASH_WRProt           0xFFF003FF

void FLASH_ROM_READ(uint32_t StartAddr, void *Buffer, uint32_t len)
{
    uint32_t  i, Length = (len + 3) >> 2;
    uint32_t *pCode = (uint32_t *)StartAddr;
    uint32_t *pBuf = (uint32_t *)Buffer;

    for(i = 0; i < Length; i++)
    {
        *pBuf++ = *pCode++;
    }
}

uint8_t UserOptionByteConfig(FunctionalState RESET_EN, FunctionalState UART_NO_KEY_EN,
                           uint32_t FLASHProt_Size)
{
    uint32_t s, t;

    FLASH_ROM_READ(0x14, &s, 4);

    if(s == 0xF3F9BDA9)
    {
        s = 0;
        FLASH_EEPROM_CMD(CMD_GET_ROM_INFO, 0x3EFFC, &s, 4);
        s &= 0xFF;

        if(RESET_EN == ENABLE)
            s |= RESET_Enable;
        else
            s &= RESET_Disable;

        s |= ((~(s << 24)) & 0xFF000000);

        if(UART_NO_KEY_EN == ENABLE)
            s |= UART_NO_KEY_Enable;

        s &= 0xFF0003FF;
        s |= ((FLASHProt_Size << 10) | (5 << 20)) & 0x00FFFC00;

        FLASH_ROM_WRITE(0x14, &s, 4);

        FLASH_ROM_READ(0x14, &t, 4);

        if(s == t)
            return 0;
        else
            return 1;
    }

    return 1;
}

uint8_t UserOptionByteClose_SWD(void)
{
    uint32_t s, t;

    return 1;
}

void UserOptionByte_Active(void)
{
    FLASH_ROM_SW_RESET();
    sys_safe_access_enable();
    R16_INT_LSI_TUNE = 0xFFFF;
    sys_safe_access_disable();
    sys_safe_access_enable();
    R8_RST_WDOG_CTRL |= RB_SOFTWARE_RESET;
    sys_safe_access_disable();
    while(1);
}

void GET_UNIQUE_ID(uint8_t *Buffer)
{
    uint16_t temp;
    FLASH_EEPROM_CMD( CMD_GET_ROM_INFO, ROM_CFG_MAC_ADDR, Buffer, 0 );
    temp = (Buffer[0]|(Buffer[1]<<8)) + (Buffer[2]|(Buffer[3]<<8)) + (Buffer[4]|(Buffer[5]<<8));
    Buffer[6] = temp&0xFF;
    Buffer[7] = (temp>>8)&0xFF;
}
