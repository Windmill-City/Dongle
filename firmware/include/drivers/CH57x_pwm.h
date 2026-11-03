#ifndef __CH57x_PWM_H__
#define __CH57x_PWM_H__

#ifdef __cplusplus
extern "C" {
#endif

#define CH_PWM1     0x01
#define CH_PWM2     0x02
#define CH_PWM3     0x04
#define CH_PWM4     0x08
#define CH_PWM5     0x10
#define CH_PWM_ALL  0x1F

typedef enum
{
    Mode_DMACH1_3 = 0,
    Mode_DMACH4_5,
    Mode_DMACH1_5,
} PWM_DMAChannel;

typedef enum
{
    High_Level = 0,
    Low_Level,
} PWMX_PolarTypeDef;

typedef enum
{
    PWMX_Cycle_256 = 0,
    PWMX_Cycle_255,
    PWMX_Cycle_128,
    PWMX_Cycle_127,
    PWMX_Cycle_64,
    PWMX_Cycle_63,
} PWMX_CycleTypeDef;

typedef enum
{
    PWM_ModeSINGLE = 0,
    PWM_ModeLOOP,
} PWM_DMAModeTypeDef;

#define PWMX_CLKCfg(d)    (R16_PWM_CLOCK_DIV = d)

void PWMX_CycleCfg(PWMX_CycleTypeDef cyc);

void PWMX_16bit_CycleCfg(uint8_t ch, uint16_t cyc);

#define PWM_16bit_CycleEnable()  (R8_PWM_CONFIG |= (3 << 1))

#define PWM_16bit_CycleDisable()  (R8_PWM_CONFIG &= ~(3 << 1))

#define PWM1_ActDataWidth(d)     (R8_PWM1_DATA = d)

#define PWM2_ActDataWidth(d)     (R8_PWM2_DATA = d)

#define PWM3_ActDataWidth(d)     (R8_PWM3_DATA = d)

#define PWM4_ActDataWidth(d)     (R8_PWM4_DATA = d)

#define PWM5_ActDataWidth(d)     (R8_PWM5_DATA = d)

#define PWM1_16bit_ActDataWidth(d)     (R16_PWM1_DATA = d)

#define PWM2_16bit_ActDataWidth(d)     (R16_PWM2_DATA = d)

#define PWM3_16bit_ActDataWidth(d)     (R16_PWM3_DATA = d)

#define PWM4_16bit_ActDataWidth(d)     (R16_PWM4_DATA = d)

#define PWM5_16bit_ActDataWidth(d)     (R16_PWM5_DATA = d)

void PWMX_ACTOUT(uint8_t ch, uint8_t da, PWMX_PolarTypeDef pr, FunctionalState s);

void PWMX_16bit_ACTOUT(uint8_t ch, uint16_t da, PWMX_PolarTypeDef pr, FunctionalState s);

void PWMX_AlterOutCfg(uint8_t ch, FunctionalState s);

void PWMX_SyncOutCfg(FunctionalState s);

void PWM_DMACfg(uint8_t s, uint32_t startAddr, uint32_t endAddr, PWM_DMAModeTypeDef m, PWM_DMAChannel ch);

#ifdef __cplusplus
}
#endif

#endif
