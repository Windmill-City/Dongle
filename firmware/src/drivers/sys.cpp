#include "common.hpp"

__HIGH_CODE
void SetSysClock(SYS_CLKTypeDef sc)
{
    uint16_t clk_sys_cfg;
    uint8_t i;
    uint8_t x32M_c;

    if(sc == RB_CLK_SYS_MOD)
    {
        sys_safe_access_enable();
        R8_SLP_POWER_CTRL |= 0x40;
        R8_CLK_SYS_CFG |= RB_CLK_SYS_MOD;
        sys_safe_access_disable();
    }
    else
    {
        if(!(R8_HFCK_PWR_CTRL & RB_CLK_XT32M_PON))
        {
            x32M_c = R8_XT32M_TUNE;
            sys_safe_access_enable();
            R8_XT32M_TUNE |= 0x03;
            R8_HFCK_PWR_CTRL |= RB_CLK_XT32M_PON;
            sys_safe_access_disable();
            clk_sys_cfg = R8_CLK_SYS_CFG;
            sys_safe_access_enable();
            R8_CLK_SYS_CFG |= 0xC0;
            sys_safe_access_disable();
            for(i=0; i<9; i++)
            {
                __nop();
            }
            sys_safe_access_enable();
            R8_CLK_SYS_CFG = clk_sys_cfg;
            R8_XT32M_TUNE = x32M_c;
            sys_safe_access_disable();
        }

        if((sc & RB_CLK_SYS_MOD) == 0x40)
        {
            sys_safe_access_enable();
            R8_HFCK_PWR_CTRL |= RB_CLK_PLL_PON;
            R8_FLASH_CFG = 0X01;
            R8_FLASH_SCK |= 1<<4;
            sys_safe_access_disable();
        }
        else
        {
            if((sc&0x1F) )
            {
                sys_safe_access_enable();
                R8_FLASH_CFG = 0X02;
                sys_safe_access_disable();
            }
            else
            {
                sys_safe_access_enable();
                R8_FLASH_CFG = 0X07;
                sys_safe_access_disable();
            }
        }

        sys_safe_access_enable();
        R8_SLP_POWER_CTRL |= 0x40;
        R8_CLK_SYS_CFG = sc;
        sys_safe_access_disable();
    }
}

uint32_t SYS_GetSysTickCnt(void)
{
    return SysTick->CNTL;
}

extern "C"
{
__INTERRUPT
__HIGH_CODE
__attribute__((weak))
void HardFault_Handler(void)
{
    sys_safe_access_enable();
    R8_RST_WDOG_CTRL |= RB_SOFTWARE_RESET;
    sys_safe_access_disable();
    while(1);
}

__INTERRUPT
__HIGH_CODE
__attribute__((weak))
void NMI_Handler(void)
{
    while(1);
}
}
