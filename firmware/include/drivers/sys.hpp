#ifndef __ch57x_SYS_HPP__
#define __ch57x_SYS_HPP__

#include <stdint.h>
#include "sfr.hpp"
#include "clk.hpp"

inline uint8_t SYS_GetChipID() { return R8_CHIP_ID; }
inline uint8_t SYS_GetAccessID() { return R8_SAFE_ACCESS_ID; }

void SetSysClock(SYS_CLKTypeDef sc);

uint32_t SYS_GetSysTickCnt(void);

#define sys_safe_access_enable()        do{volatile uint32_t mpie_mie;mpie_mie=__risc_v_disable_irq();SAFEOPERATE;\
                                        R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;SAFEOPERATE;

#define sys_safe_access_disable()       R8_SAFE_ACCESS_SIG = 0;__risc_v_enable_irq(mpie_mie);SAFEOPERATE;}while(0)

#endif
