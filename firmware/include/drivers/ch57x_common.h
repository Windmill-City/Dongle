#ifndef __ch57x_COMM_H__
#define __ch57x_COMM_H__

#ifdef __cplusplus
 extern "C" {
#endif

#ifndef  NULL
#define  NULL     0
#endif
#define  ALL			0xFFFF

#ifndef __HIGH_CODE
#define __HIGH_CODE   __attribute__((section(".highcode")))
#endif

#ifndef __INTERRUPT
#ifdef INT_SOFT
#define __INTERRUPT   __attribute__((interrupt()))
#else
#define __INTERRUPT   __attribute__((interrupt("WCH-Interrupt-fast")))
#endif
#endif

#ifndef	 FREQ_SYS
#define  FREQ_SYS		100000000
#endif

#ifndef  SAFEOPERATE
#define  SAFEOPERATE   asm volatile("fence.i")
#endif

#include <string.h>
#include <stdint.h>
#include <ch572sfr.h>
#include "core_riscv.h"
#include "ch57x_clk.h"
#include "ch57x_gpio.h"
#include "ch57x_sys.h"
#include "ch57x_timer.h"
#include "ch57x_usbdev.h"
#include "isp572.h"

 extern uint32_t Freq_LSI;

#define DelayMs(x)      mDelaymS(x)
#define DelayUs(x)      mDelayuS(x)

#define ROM_CFG_VERISON    0x7F010

#ifdef __cplusplus
}
#endif

#endif
