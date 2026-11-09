#pragma once

#include <cstdint>

namespace ch572::rfend {

// RX filter calibration results shared with the radio driver.
namespace detail {
inline std::uint8_t g_tune_filter = 0;
inline std::uint8_t g_tune_filter_2m = 0;
} // namespace detail

inline std::uint8_t tune_filter() noexcept { return detail::g_tune_filter; }
inline std::uint8_t tune_filter_2m() noexcept { return detail::g_tune_filter_2m; }

// Low-level RF front-end calibration block.
void dev_init() noexcept;
void calibration() noexcept;
void tx_ctune() noexcept;
void tx_ftune() noexcept;
void rx_filter() noexcept;
void rx_adc() noexcept;
void wait_tune(std::uint16_t cycles) noexcept;

} // namespace ch572::rfend
