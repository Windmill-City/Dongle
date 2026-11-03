#include "CH57x_common.h"

void KeyScan_Cfg(uint8_t s, uint16_t keyScanPin, uint16_t ClkDiv, uint16_t Rep)
{
    if(s == DISABLE)
    {
        R16_KEY_SCAN_CTRL &= ~(RB_SCAN_START_EN);
    }
    else
    {
        R16_KEY_SCAN_CTRL |= keyScanPin | ClkDiv | Rep;
        R16_KEY_SCAN_CTRL |= RB_SCAN_START_EN;
    }
}

void KeyPress_Wake(uint8_t s)
{
    if(s == DISABLE)
    {
        sys_safe_access_enable();
        R8_SLP_CLK_OFF0 &= ~(RB_SLP_KEYSCAN_WAKE);
        sys_safe_access_disable();
    }
    else
    {
        sys_safe_access_enable();
        R8_SLP_CLK_OFF0 |= RB_SLP_KEYSCAN_WAKE;
        sys_safe_access_disable();
    }
}
