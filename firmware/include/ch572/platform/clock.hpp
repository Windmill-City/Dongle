#pragma once

#include <cstdint>

namespace ch572::clock {

enum class HseCapacitance : std::uint8_t
{
    p6 = 0,
    p8,
    p10,
    p12,
    p14,
    p16,
    p18,
    p20,
};

void set_hse_capacitance(HseCapacitance capacitance) noexcept;

// Configure HSE (32 MHz) and the PLL for a 100 MHz system clock.
void init_100mhz() noexcept;

} // namespace ch572::clock
