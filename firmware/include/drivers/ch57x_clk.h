#ifndef __ch57x_CLK_H__
#define __ch57x_CLK_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
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

} SYS_CLKTypeDef;

typedef enum
{
    HSE_RCur_75 = 0,
    HSE_RCur_100,
    HSE_RCur_125,
    HSE_RCur_150

} HSECurrentTypeDef;

typedef enum
{
    HSECap_6p = 0,
    HSECap_8p,
    HSECap_10p,
    HSECap_12p,
    HSECap_14p,
    HSECap_16p,
    HSECap_18p,
    HSECap_20p

} HSECapTypeDef;

#define RTC_MAX_COUNT             0xA8C00000

#define MAX_DAY                   0x00004000
#define MAX_2_SEC                 0x0000A8C0

#define BEGYEAR                   2020
#define IsLeapYear(yr)            (!((yr) % 400) || (((yr) % 100) && !((yr) % 4)))
#define YearLength(yr)            (IsLeapYear(yr) ? 366 : 365)
#define monthLength(lpyr, mon)    (((mon) == 1) ? (28 + (lpyr)) : (((mon) > 6) ? (((mon) & 1) ? 31 : 30) : (((mon) & 1) ? 30 : 31)))

typedef enum
{
    Count_1 = 0,
    Count_2,
    Count_4,
    Count_32,
    Count_64,
    Count_128,
    Count_1024,
    Count_2047,
} RTC_OSCCntTypeDef;

typedef enum
{
    Period_4096 = 0,
    Period_8192,
    Period_16384,
    Period_32768,
    Period_65536,
    Period_131072,
    Period_262144,
    Period_524288,
} RTC_TMRCycTypeDef;

typedef enum
{
    RTC_TRIG_EVENT = 0,
    RTC_TMR_EVENT,

} RTC_EVENTTypeDef;

typedef enum
{
    RTC_TRIG_MODE = 0,
    RTC_TMR_MODE,

} RTC_MODETypeDef;

void HSECFG_Current(HSECurrentTypeDef c);

void HSECFG_Capacitance(HSECapTypeDef c);

uint32_t RTC_InitClock(RTC_OSCCntTypeDef cnt);

void RTC_InitTime(uint16_t y, uint16_t mon, uint16_t d, uint16_t h, uint16_t m, uint16_t s);

void RTC_GetTime(uint16_t *py, uint16_t *pmon, uint16_t *pd, uint16_t *ph, uint16_t *pm, uint16_t *ps);

void RTC_SetCycleLSI(uint32_t cyc);

uint32_t RTC_GetCycleLSI(void);

void RTC_TRIGFunCfg(uint32_t cyc);

void RTC_TMRFunCfg(RTC_TMRCycTypeDef t);

void RTC_ModeFunDisable(RTC_MODETypeDef m);

uint8_t RTC_GetITFlag(RTC_EVENTTypeDef f);

void RTC_ClearITFlag(RTC_EVENTTypeDef f);

void LClk_Cfg(FunctionalState s);

#ifdef __cplusplus
}
#endif

#endif
