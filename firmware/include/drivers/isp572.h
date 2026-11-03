#ifndef FLASH_MIN_WR_SIZE
#define FLASH_MIN_WR_SIZE   4
#endif
#ifndef FLASH_ROM_MAX_SIZE
#define FLASH_ROM_MAX_SIZE  0x03C000
#endif
#ifndef FLASH_BLOCK_SIZE
#define FLASH_BLOCK_SIZE    4096
#endif

#ifndef CMD_FLASH_ROM_SW_RESET
#define CMD_FLASH_ROM_START_IO  0x00
#define CMD_FLASH_ROM_SW_RESET  0x04
#define CMD_GET_ROM_INFO        0x06
#define CMD_GET_UNIQUE_ID       0x07
#define CMD_FLASH_ROM_PWR_DOWN  0x0D
#define CMD_FLASH_ROM_PWR_UP    0x0C
#define CMD_FLASH_ROM_ERASE     0x01
#define CMD_FLASH_ROM_WRITE     0x02
#define CMD_FLASH_ROM_VERIFY    0x03
#endif

#define ROM_CFG_MAC_ADDR        0x3F018
#define ROM_CFG_BOOT_INFO       0x3DFF8

extern uint32_t FLASH_EEPROM_CMD( uint8_t cmd, uint32_t StartAddr, void *Buffer, uint32_t Length );

#define FLASH_ROM_START_IO( )                       FLASH_EEPROM_CMD( CMD_FLASH_ROM_START_IO, 0, NULL, 0 )

#define FLASH_ROM_SW_RESET( )                       FLASH_EEPROM_CMD( CMD_FLASH_ROM_SW_RESET, 0, NULL, 0 )

#define GetMACAddress(Buffer)                       FLASH_EEPROM_CMD( CMD_GET_ROM_INFO, ROM_CFG_MAC_ADDR, Buffer, 0 )

#define GET_BOOT_INFO(Buffer)                       FLASH_EEPROM_CMD( CMD_GET_ROM_INFO, ROM_CFG_BOOT_INFO, Buffer, 0 )

#define FLASH_ROM_PWR_DOWN( )                       FLASH_EEPROM_CMD( CMD_FLASH_ROM_PWR_DOWN, 0, NULL, 0 )

#define FLASH_ROM_PWR_UP( )                         FLASH_EEPROM_CMD( CMD_FLASH_ROM_PWR_UP, 0, NULL, 0 )

#define FLASH_ROM_ERASE(StartAddr,Length)           FLASH_EEPROM_CMD( CMD_FLASH_ROM_ERASE, StartAddr, NULL, Length )

#define FLASH_ROM_WRITE(StartAddr,Buffer,Length)    FLASH_EEPROM_CMD( CMD_FLASH_ROM_WRITE, StartAddr, Buffer, Length )

#define FLASH_ROM_VERIFY(StartAddr,Buffer,Length)   FLASH_EEPROM_CMD( CMD_FLASH_ROM_VERIFY, StartAddr, Buffer, Length )
