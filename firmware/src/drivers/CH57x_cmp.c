#include "CH57x_common.h"

void CMP_Init (CMPSwTypeDef s, CMPNrefLevelTypeDef v)
{
    R8_CMP_CTRL_0 = (s << 2)|(v << 4);
}

void CMP_OutToTIMCAPCfg(FunctionalState s)
{
    if (s)
    {
        R8_CMP_CTRL_0 |= RB_CMP_CAP;
    }
    else
    {
        R8_CMP_CTRL_0 &= ~RB_CMP_CAP;
    }
}

void CMP_INTCfg (CMPOutSelTypeDef sel, FunctionalState s)
{
    R8_CMP_CTRL_1 = (sel << 2);
    if (s)
    {
        R8_CMP_CTRL_1 |= RB_CMP_IE;
    }
    else
    {
        R8_CMP_CTRL_1 &= ~RB_CMP_IE;
    }
}
