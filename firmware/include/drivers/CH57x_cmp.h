#ifndef __CH57x_CMP_H__
#define __CH57x_CMP_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    cmp_sw_0 = 0,
    cmp_sw_1,
    cmp_sw_2,
    cmp_sw_3
} CMPSwTypeDef;

typedef enum
{
    cmp_nref_level_50 = 0,
    cmp_nref_level_100,
    cmp_nref_level_150,
    cmp_nref_level_200,
    cmp_nref_level_250,
    cmp_nref_level_300,
    cmp_nref_level_350,
    cmp_nref_level_400,
    cmp_nref_level_450,
    cmp_nref_level_500,
    cmp_nref_level_550,
    cmp_nref_level_600,
    cmp_nref_level_650,
    cmp_nref_level_700,
    cmp_nref_level_750,
    cmp_nref_level_800,
} CMPNrefLevelTypeDef;

typedef enum
{
    cmp_out_sel_high = 0,
    cmp_out_sel_low,
    cmp_out_sel_fall,
    cmp_out_sel_rise
} CMPOutSelTypeDef;

#define CMP_GetITStatus() (R8_CMP_CTRL_2 & RB_CMP_IF)

#define CMP_ClearITStatus() (R8_CMP_CTRL_2 |= RB_CMP_IF)

#define CMP_ReadAPROut() (R8_CMP_CTRL_3 & RB_APR_OUT_CMP)

#define CMP_Enable() (R8_CMP_CTRL_0 |= RB_CMP_EN)

#define CMP_Disable() (R8_CMP_CTRL_0 &= ~RB_CMP_EN)

void CMP_Init (CMPSwTypeDef s, CMPNrefLevelTypeDef v);

void CMP_OutToTIMCAPCfg(FunctionalState s);

void CMP_INTCfg (CMPOutSelTypeDef sel, FunctionalState s);

#ifdef __cplusplus
}
#endif

#endif
