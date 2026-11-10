#pragma once

#include <cstdint>

namespace ch572::rfend
{

// RX filter calibration result shared with the radio driver.
namespace detail
{
inline std::uint8_t g_tune_filter_2m = 0;
} // namespace detail

inline std::uint8_t tune_filter_2m() noexcept
{
    return detail::g_tune_filter_2m;
}

// Low-level RF front-end calibration block.
void init() noexcept;
void calibration() noexcept;

} // namespace ch572::rfend
