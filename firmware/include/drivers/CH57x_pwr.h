#ifndef __CH57x_PWR_H__
#define __CH57x_PWR_H__

#ifdef __cplusplus
extern "C" {
#endif

#define ROM_CFG_ADR_HW       0x7F00C

typedef enum
{
    Fsys_Delay_3584 = 0,
    Fsys_Delay_512,
    Fsys_Delay_64,
    Fsys_Delay_1,
    Fsys_Delay_8191,
    Fsys_Delay_7168,
    Fsys_Delay_6144,
    Fsys_Delay_4096,

} WakeUP_ModeypeDef;

typedef enum
{

    LPLevel_1V8 = 0,
    LPLevel_2V0,
    LPLevel_2V2,
    LPLevel_2V4,

} VolM_LevelypeDef;

void PWR_PeriphClkCfg(FunctionalState s, uint16_t perph);

void PWR_PeriphWakeUpCfg(FunctionalState s, uint8_t perph, WakeUP_ModeypeDef mode);

void PowerMonitor(FunctionalState s, VolM_LevelypeDef vl);

void LowPower_Idle(void);

void LowPower_Halt(void);

void LowPower_Sleep(uint16_t rm);

void LowPower_Shutdown(uint16_t rm);

#ifdef __cplusplus
}
#endif

#endif
