#ifndef __CH57x_TIMER_H__
#define __CH57x_TIMER_H__

#ifdef __cplusplus
extern "C" {
#endif

#define DataBit_25            (1 << 25)

#define TMR_IT_CYC_END     0x01
#define TMR_IT_DATA_ACT    0x02
#define TMR_IT_FIFO_HF     0x04
#define TMR_IT_DMA_END     0x08
#define TMR_IT_FIFO_OV     0x10

#define RB_IE_DIR_INC      0x01
#define RB_IE_DIR_DEC      0x02

typedef enum
{
    PWM_Times_1 = 0,
    PWM_Times_4,
    PWM_Times_8,
    PWM_Times_16,
} PWM_RepeatTsTypeDef;

typedef enum
{
    CAP_NULL = 0,
    Edge_To_Edge,
    FallEdge_To_FallEdge,
    RiseEdge_To_RiseEdge,
} CapModeTypeDef;

typedef enum
{
    Mode_Single = 0,
    Mode_LOOP,
} DMAModeTypeDef;

typedef enum
{
    Mode_IDLE = 0,
    Mode_T2 ,
    Mode_T1 ,
    Mode_T1T2 ,
} ENCModeTypeDef;

void TMR_TimerInit(uint32_t t);

void TMR_EXTSingleCounterInit(CapModeTypeDef cap);

#define TMR_CountOverflowCfg(cyc)    (R32_TMR_CNT_END = (cyc + 2))

#define TMR_GetCurrentCount()        R32_TMR_COUNT

#define TMR_PWMCycleCfg(cyc)         (R32_TMR_CNT_END = cyc)

void TMR_PWMInit(PWMX_PolarTypeDef pr, PWM_RepeatTsTypeDef ts);

#define TMR_PWMActDataWidth(d)    (R32_TMR_FIFO = d)

#define TMR_CAPTimeoutCfg(cyc)    (R32_TMR_CNT_END = cyc)

void TMR_CapInit(CapModeTypeDef cap);

#define TMR_CAPGetData()        R32_TMR_FIFO

#define TMR_CAPDataCounter()    R8_TMR_FIFO_COUNT

void TMR_DMACfg(uint8_t s, uint32_t startAddr, uint32_t endAddr, DMAModeTypeDef m);

void ENC_Config(uint8_t s, uint32_t encReg, ENCModeTypeDef m);

#define TMR_PWMDisable()           (R8_TMR_CTRL_MOD &= ~RB_TMR_OUT_EN)

#define TMR_PWMEnable()           (R8_TMR_CTRL_MOD |= RB_TMR_OUT_EN)

#define TMR_Disable()           (R8_TMR_CTRL_MOD &= ~RB_TMR_COUNT_EN)

#define TMR_Enable()            (R8_TMR_CTRL_MOD |= RB_TMR_COUNT_EN)

#define TMR_ITCfg(s, f)         ((s) ? (R8_TMR_INTER_EN |= f) : (R8_TMR_INTER_EN &= ~f))

#define TMR_ClearITFlag(f)      (R8_TMR_INT_FLAG = f)

#define TMR_GetITFlag(f)        (R8_TMR_INT_FLAG & f)

#define ENC_GetCurrentDir       (R8_ENC_REG_CTRL>>5 & 0x01)

#define ENC_GetCurrentCount      R32_ENC_REG_CCNT

#define ENC_GetCountandReset()  R8_ENC_REG_CTRL |= RB_RD_CLR_EN

#define ENC_ITCfg(s, f)         ((s) ? (R8_ENC_INTER_EN |= f) : (R8_ENC_INTER_EN &= ~f))

#define ENC_ClearITFlag(f)      (R8_ENC_INT_FLAG = f)

#define ENC_GetITFlag(f)        (R8_ENC_INT_FLAG & f)

#ifdef __cplusplus
}
#endif

#endif
