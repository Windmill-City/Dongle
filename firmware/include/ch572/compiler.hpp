#pragma once

#if defined(__GNUC__)
  #define CH572_HIGH_CODE __attribute__((section(".highcode")))
  #define CH572_INTERRUPT __attribute__((interrupt("WCH-Interrupt-fast")))
  #define CH572_PACKED    __attribute__((packed))
#else
  #define CH572_HIGH_CODE
  #define CH572_INTERRUPT
  #define CH572_PACKED
#endif
