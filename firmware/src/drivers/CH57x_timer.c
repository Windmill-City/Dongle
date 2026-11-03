#include "CH57x_common.h"

void TMR_TimerInit(uint32_t t)
{
    R32_TMR_CNT_END = t;
    R8_TMR_CTRL_MOD = RB_TMR_ALL_CLEAR;
    R8_TMR_CTRL_MOD = RB_TMR_COUNT_EN;
}

void TMR_EXTSingleCounterInit(CapModeTypeDef cap)
{
    R8_TMR_CTRL_MOD = RB_TMR_ALL_CLEAR;
    R8_TMR_CTRL_MOD = RB_TMR_COUNT_EN | RB_TMR_CAP_COUNT | RB_TMR_MODE_IN | (cap << 6);
}

void TMR_PWMInit(PWMX_PolarTypeDef pr, PWM_RepeatTsTypeDef ts)
{
    R8_TMR_CTRL_MOD = RB_TMR_ALL_CLEAR;
    R8_TMR_CTRL_MOD = (pr << 4) | (ts << 6);
}

void TMR_CapInit(CapModeTypeDef cap)
{
    R8_TMR_CTRL_MOD = RB_TMR_ALL_CLEAR;
    R8_TMR_CTRL_MOD = RB_TMR_COUNT_EN | RB_TMR_MODE_IN | (cap << 6);
}

void TMR_DMACfg(uint8_t s, uint32_t startAddr, uint32_t endAddr, DMAModeTypeDef m)
{
    if(s == DISABLE)
    {
        R8_TMR_CTRL_DMA = 0;
    }
    else
    {
        R16_TMR_DMA_BEG = startAddr & 0xFFFF;
        R16_TMR_DMA_END = endAddr & 0xFFFF;
        if(m)
            R8_TMR_CTRL_DMA = RB_TMR_DMA_LOOP | RB_TMR_DMA_ENABLE;
        else
            R8_TMR_CTRL_DMA = RB_TMR_DMA_ENABLE;
    }
}

void ENC_Config(uint8_t s, uint32_t encReg, ENCModeTypeDef m)
{
    if(s == DISABLE)
    {
        R8_ENC_REG_CTRL &= ~(RB_START_ENC_EN);
    }
    else
    {
        R8_ENC_REG_CTRL |= (m << 1) | RB_START_ENC_EN;
        R32_ENC_REG_CEND |= encReg;
    }
}
