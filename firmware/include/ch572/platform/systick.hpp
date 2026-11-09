#pragma once

#include <cstdint>

#include "ch572/platform/registers.hpp"

namespace ch572::systick {

// System-clock cycles per millisecond. Updated by start() so
// delay_ms() follows the current clock without hard-coding it at each call.
inline std::uint32_t g_ticks_per_ms = 100000u;

// Start the free-running 32-bit down-counter at the system clock.  No
// interrupt is enabled; callers poll now().
void start(std::uint32_t ticks_per_ms) noexcept;

inline std::uint32_t now() noexcept
{
    return systick_regs().cntl;
}

// Block for `ms` milliseconds using the free-running counter.
inline void delay_ms(std::uint32_t ms) noexcept
{
    const std::uint32_t ticks = ms * g_ticks_per_ms;
    const std::uint32_t start = now();
    while ((start - now()) < ticks)
    {
    }
}

} // namespace ch572::systick
