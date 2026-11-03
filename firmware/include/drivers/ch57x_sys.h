#ifndef __ch57x_SYS_H__
#define __ch57x_SYS_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    RST_STATUS_SW = 0,
    RST_STATUS_RPOR,
    RST_STATUS_WTR,
    RST_STATUS_MR,
    RST_STATUS_LRM0,
    RST_STATUS_GPWSM,
    RST_STATUS_LRM1,
    RST_STATUS_LRM2,

} SYS_ResetStaTypeDef;

typedef enum
{
    INFO_RESET_EN = 0x4,
    INFO_BOOT_EN = 0x8,
    INFO_RST_PIN = 0x10,
    INFO_LOADER  = 0x20,
    STA_SAFEACC_ACT = 0x30,

} SYS_InfoStaTypeDef;

#define SYS_GetChipID()      R8_CHIP_ID

#define SYS_GetAccessID()    R8_SAFE_ACCESS_ID

void SetSysClock(SYS_CLKTypeDef sc);

uint32_t GetSysClock(void);

uint8_t SYS_GetInfoSta(SYS_InfoStaTypeDef i);

#define SYS_GetLastResetSta()    (R8_RESET_STATUS & RB_RESET_FLAG)

void SYS_ResetExecute(void);

#define SYS_ResetKeepBuf(d)    (R8_GLOB_RESET_KEEP = d)

void SYS_DisableAllIrq(uint32_t *pirqv);

void SYS_RecoverIrq(uint32_t irq_status);

uint32_t SYS_GetSysTickCnt(void);

#define WWDG_SetCounter(c)    (R8_WDOG_COUNT = c)

void WWDG_ITCfg(FunctionalState s);

void WWDG_ResetCfg(FunctionalState s);

#define WWDG_GetFlowFlag()    (R8_RST_WDOG_CTRL & RB_WDOG_INT_FLAG)

void WWDG_ClearFlag(void);

void mDelayuS(uint16_t t);

void mDelaymS(uint16_t t);

#define sys_safe_access_enable()        do{volatile uint32_t mpie_mie;mpie_mie=__risc_v_disable_irq();SAFEOPERATE;\
                                        R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;SAFEOPERATE;

#define sys_safe_access_disable()       R8_SAFE_ACCESS_SIG = 0;__risc_v_enable_irq(mpie_mie);SAFEOPERATE;}while(0)

#ifdef __cplusplus
}
#endif

#endif
