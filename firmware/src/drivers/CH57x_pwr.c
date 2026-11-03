#include "CH57x_common.h"

void PWR_PeriphClkCfg(FunctionalState s, uint16_t perph)
{
    uint32_t sleep_ctrl = R32_SLEEP_CONTROL;

    if(s == DISABLE)
    {
        sleep_ctrl |= perph;
    }
    else
    {
        sleep_ctrl &= ~perph;
    }

    sys_safe_access_enable();
    R32_SLEEP_CONTROL = sleep_ctrl;
    sys_safe_access_disable();
}

void PWR_PeriphWakeUpCfg(FunctionalState s, uint8_t perph, WakeUP_ModeypeDef mode)
{
    if(s == DISABLE)
    {
        sys_safe_access_enable();
        R8_SLP_WAKE_CTRL &= ~perph;
        sys_safe_access_disable();
    }
    else
    {
        sys_safe_access_enable();
        R8_SLP_WAKE_CTRL |= RB_WAKE_EV_MODE | perph;
        sys_safe_access_disable();
        sys_safe_access_enable();
        R8_SLP_POWER_CTRL &= ~(RB_WAKE_DLY_MOD);
        sys_safe_access_disable();
        sys_safe_access_enable();
        R8_SLP_POWER_CTRL |= mode;
        sys_safe_access_disable();
    }
}

void PowerMonitor(FunctionalState s, VolM_LevelypeDef vl)
{
    uint8_t ctrl = R8_BAT_DET_CTRL;
    uint8_t cfg = R8_BAT_DET_CFG;

    if(s == DISABLE)
    {
        sys_safe_access_enable();
        R8_BAT_DET_CTRL = 0;
        sys_safe_access_disable();
    }
    else
    {
        cfg = vl & 0x03;
        ctrl = RB_BAT_MON_EN;
        sys_safe_access_enable();
        R8_BAT_DET_CTRL = ctrl;
        R8_BAT_DET_CFG = cfg;
        sys_safe_access_disable();

        mDelayuS(1);
        sys_safe_access_enable();
        R8_BAT_DET_CTRL |= RB_BAT_LOW_IE;
        sys_safe_access_disable();
    }
}

__HIGH_CODE
void LowPower_Idle(void)
{
    FLASH_ROM_SW_RESET();
    R8_FLASH_CTRL = 0x04;

    PFIC->SCTLR &= ~(1 << 2);
    __WFI();
    __nop();
    __nop();
}

__HIGH_CODE
void LowPower_Halt(void)
{
    uint8_t x32Mpw;

    FLASH_ROM_SW_RESET();
    R8_FLASH_CTRL = 0x04;
    x32Mpw = R8_XT32M_TUNE;
    if(!(R8_HFCK_PWR_CTRL&RB_CLK_XT32M_KEEP))
    {
        x32Mpw = (x32Mpw & 0xfc) | 0x03;
    }

    sys_safe_access_enable();
    R8_BAT_DET_CTRL = 0;
    sys_safe_access_disable();
    sys_safe_access_enable();
    R8_XT32M_TUNE = x32Mpw;
    sys_safe_access_disable();
    sys_safe_access_enable();
    R8_PLL_CONFIG |= (1 << 5);
    sys_safe_access_disable();

    PFIC->SCTLR |= (1 << 2);
    __WFI();
    __nop();
    __nop();
    sys_safe_access_enable();
    R8_PLL_CONFIG &= ~(1 << 5);
    sys_safe_access_disable();
}

__HIGH_CODE
void LowPower_Sleep(uint16_t rm)
{
    uint8_t x32Mpw;
    uint16_t power_plan;
    uint8_t clk_sys_cfg;

    clk_sys_cfg = R8_CLK_SYS_CFG;
    x32Mpw = R8_XT32M_TUNE;
    x32Mpw = (x32Mpw & 0xfc) | 0x03;

    sys_safe_access_enable();
    R8_BAT_DET_CTRL = 0;
    sys_safe_access_disable();
    sys_safe_access_enable();
    R8_XT32M_TUNE = x32Mpw;
    sys_safe_access_disable();

    PFIC->SCTLR |= (1 << 2);

    power_plan = RB_PWR_PLAN_EN | RB_PWR_CORE | rm | (2<<11);

    sys_safe_access_enable();

    R16_POWER_PLAN = power_plan;
    sys_safe_access_disable();
    sys_safe_access_enable();
    R8_CLK_SYS_CFG = CLK_SOURCE_HSE_PLL_24MHz;
    sys_safe_access_disable();

    __WFI();
    __nop();
    __nop();

    sys_safe_access_enable();
    R8_CLK_SYS_CFG = clk_sys_cfg;
    sys_safe_access_disable();
    sys_safe_access_enable();
    R16_POWER_PLAN &= ~RB_PWR_PLAN_EN;
    R16_POWER_PLAN &= ~RB_XT_PRE_EN;
    sys_safe_access_disable();

    DelayUs(40);
}

__HIGH_CODE
void LowPower_Shutdown(uint16_t rm)
{
    uint8_t x32Mpw;

    FLASH_ROM_SW_RESET();
    x32Mpw = R8_XT32M_TUNE;
    x32Mpw = (x32Mpw & 0xfc) | 0x03;

    sys_safe_access_enable();
    R8_BAT_DET_CTRL = 0;
    sys_safe_access_disable();
    sys_safe_access_enable();
    R8_XT32M_TUNE = x32Mpw;
    sys_safe_access_disable();

    PFIC->SCTLR |= (1 << 2);

    SetSysClock(CLK_SOURCE_HSE_PLL_24MHz);
    sys_safe_access_enable();
    R16_POWER_PLAN = RB_PWR_PLAN_EN | rm;
    sys_safe_access_disable();
    __WFI();
    __nop();
    __nop();
    FLASH_ROM_SW_RESET();
    sys_safe_access_enable();
    R16_INT_LSI_TUNE = 0xFFFF;
    R8_RST_WDOG_CTRL |= RB_SOFTWARE_RESET;
    sys_safe_access_disable();
}
