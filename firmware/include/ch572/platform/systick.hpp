#pragma once

#include <cstdint>

namespace ch572::systick {

// Start the free-running 32-bit down-counter at the system clock.  No
// interrupt is enabled; callers poll now().
void start_free_running() noexcept;

// Start a periodic SysTick interrupt every `ticks` system-clock cycles.
// The handler is responsible for clearing the SysTick status flag.
void start_periodic(std::uint32_t ticks) noexcept;

std::uint32_t now() noexcept;

} // namespace ch572::systick
