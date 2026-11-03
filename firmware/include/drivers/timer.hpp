#pragma once

#include <stdint.h>
#include "sfr.hpp"
#include "core_riscv.hpp"

inline constexpr uint8_t TMR_IT_CYC_END = 0x01;

inline void TMR_ITCfg(FunctionalState s, uint8_t f)
{
    if (s)
    {
        R8_TMR_INTER_EN |= f;
    }
    else
    {
        R8_TMR_INTER_EN &= ~f;
    }
}
