#pragma once

#include <cstdint>

namespace ch572::gpio {

enum class Pin : std::uint32_t
{
    p0 = 1u << 0,
    p1 = 1u << 1,
    p2 = 1u << 2,
    p3 = 1u << 3,
    p4 = 1u << 4,
    p5 = 1u << 5,
    p6 = 1u << 6,
    p7 = 1u << 7,
    p8 = 1u << 8,
    p9 = 1u << 9,
    p10 = 1u << 10,
    p11 = 1u << 11,
    p12 = 1u << 12,
    p13 = 1u << 13,
    p14 = 1u << 14,
    p15 = 1u << 15,
};

enum class Mode
{
    input_floating,
    input_pull_up,
    input_pull_down,
    output_push_pull_5ma,
    output_push_pull_20ma,
};

void configure(Pin pin, Mode mode) noexcept;
void set(std::uint32_t pins) noexcept;
void clear(std::uint32_t pins) noexcept;

inline void set(Pin pin) noexcept
{
    set(static_cast<std::uint32_t>(pin));
}

inline void clear(Pin pin) noexcept
{
    clear(static_cast<std::uint32_t>(pin));
}

} // namespace ch572::gpio
