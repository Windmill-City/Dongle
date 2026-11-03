#ifndef __ch57x_COMM_HPP__
#define __ch57x_COMM_HPP__

#include <stdint.h>
#include <string.h>

#ifndef  NULL
#define  NULL     0
#endif

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

#ifndef FREQ_SYS
#define FREQ_SYS     100000000
#endif

#ifndef SAFEOPERATE
#define SAFEOPERATE  asm volatile("fence.i")
#endif

#include <sfr.hpp>
#include "core_riscv.hpp"
#include "clk.hpp"
#include "gpio.hpp"
#include "sys.hpp"
#include "timer.hpp"

inline constexpr uint16_t ALL = 0xFFFF;

#endif
