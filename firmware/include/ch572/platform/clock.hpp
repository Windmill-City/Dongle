#pragma once

#include <cstdint>

namespace ch572::clock
{

// Configure HSE (32 MHz) and the PLL for a 100 MHz system clock.
void                           init() noexcept;

inline constexpr std::uint32_t kSysClockHz        = 100'000'000u;

inline constexpr std::uint8_t  kSysClockSelectHse = 0xC0;
inline constexpr std::uint8_t  kSysClockSelectPll = 0x40;
inline constexpr std::uint8_t  kPll               = kSysClockSelectPll | 6; // 100MHz
inline constexpr std::uint8_t  kPowerPlanEnable   = 0x40;
inline constexpr std::uint8_t  kFlashPllConfig    = 0x01;
inline constexpr std::uint8_t  kFlashSckPll       = 1u << 4;
inline constexpr std::uint8_t  kHseCapacitance    = 0x10; // 8pF

// The core SysTick runs free-running from HCLK/8 (STK_CTLR.STCLK = 0).
inline constexpr float         kSysTickHz         = kSysClockHz / 8u;
inline constexpr float         kTicksPerMs        = kSysTickHz / 1000u;
inline constexpr float         kTicksPerUs        = kSysTickHz / 1000000u;

} // namespace ch572::clock
