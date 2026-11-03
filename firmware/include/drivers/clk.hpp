#ifndef __ch57x_CLK_HPP__
#define __ch57x_CLK_HPP__

#include <stdint.h>

enum SYS_CLKTypeDef : uint8_t
{
    CLK_SOURCE_LSI = 0xC0,

    CLK_SOURCE_HSE_16MHz = (0x02),
    CLK_SOURCE_HSE_8MHz = (0x04),
    CLK_SOURCE_HSE_6_4MHz = (0x05),
    CLK_SOURCE_HSE_4MHz = (0x08),
    CLK_SOURCE_HSE_2MHz = (0x10),
    CLK_SOURCE_HSE_1MHz = (0x0),

    CLK_SOURCE_HSE_PLL_100MHz = (0x40 | 6),
    CLK_SOURCE_HSE_PLL_75MHz = (0x40 | 8),
    CLK_SOURCE_HSE_PLL_60MHz = (0x40 | 10),
    CLK_SOURCE_HSE_PLL_50MHz = (0x40 | 12),
    CLK_SOURCE_HSE_PLL_40MHz = (0x40 | 15),
    CLK_SOURCE_HSE_PLL_30MHz = (0x40 | 20),
    CLK_SOURCE_HSE_PLL_25MHz = (0x40 | 24),
    CLK_SOURCE_HSE_PLL_24MHz = (0x40 | 25),
    CLK_SOURCE_HSE_PLL_20MHz = (0x40 | 30),
};

enum HSECapTypeDef : uint8_t
{
    HSECap_6p = 0,
    HSECap_8p,
    HSECap_10p,
    HSECap_12p,
    HSECap_14p,
    HSECap_16p,
    HSECap_18p,
    HSECap_20p,
};

void HSECFG_Capacitance(HSECapTypeDef c);

#endif
