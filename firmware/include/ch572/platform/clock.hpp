#pragma once

#include <cstdint>

namespace ch572::clock
{

void                           init() noexcept;

inline constexpr std::uint32_t kSysClockHz        = 100'000'000u;

inline constexpr std::uint8_t  kSysClockSelectHse = 0xC0;
inline constexpr std::uint8_t  kSysClockSelectPll = 0x40;
inline constexpr std::uint8_t  kPll               = kSysClockSelectPll | 6;
inline constexpr std::uint8_t  kPowerPlanEnable   = 0x40;
inline constexpr std::uint8_t  kFlashPllConfig    = 0x01;
inline constexpr std::uint8_t  kFlashSckPll       = 1u << 4;
inline constexpr std::uint8_t  kHseCapacitance    = 0x10;

inline constexpr float         kSysTickHz         = kSysClockHz / 8u;
inline constexpr float         kTicksPerMs        = kSysTickHz / 1000u;
inline constexpr float         kTicksPerUs        = kSysTickHz / 1000000u;

}
