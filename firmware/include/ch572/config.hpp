#pragma once

#include <cstdint>

#if defined(__GNUC__)
#define CH572_HIGH_CODE __attribute__((section(".highcode")))
#define CH572_INTERRUPT __attribute__((interrupt("WCH-Interrupt-fast")))
#define CH572_PACKED __attribute__((packed))
#else
#define CH572_HIGH_CODE
#define CH572_INTERRUPT
#define CH572_PACKED
#endif

namespace ch572 {

// Flash layout shared by the application and the IAP bootloader.
inline constexpr std::uint32_t kIapStart = 0x00000000u;
inline constexpr std::uint32_t kIapEnd = 0x00002000u;
inline constexpr std::uint32_t kAppStart = 0x00002000u;
inline constexpr std::uint32_t kAppEnd = 0x0003C000u;
inline constexpr std::uint32_t kFlashEnd = kAppEnd;
inline constexpr std::uint32_t kFlashSectorSize = 0x1000u;

} // namespace ch572
