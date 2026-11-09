#pragma once

#include <cstdint>

namespace ch572::rfend {

// Low-level RF front-end calibration block.
void dev_init() noexcept;
void calibration() noexcept;
void tx_ctune() noexcept;
void tx_ftune() noexcept;
void rx_filter() noexcept;
bool rx_filter_timed_out() noexcept;
std::uint32_t timer_probe() noexcept;
void rx_adc() noexcept;
void wait_tune(std::uint16_t cycles) noexcept;

void set_dacref_tune(std::uint8_t value) noexcept;
std::uint8_t get_dacref_tune() noexcept;
void set_ex_power() noexcept;

void tx_ctune_set(const std::uint8_t *values) noexcept;
void tx_ctune_get(std::uint8_t *values) noexcept;

std::uint8_t tune_filter() noexcept;
std::uint8_t tune_filter_2m() noexcept;
std::uint32_t wait_recv_time() noexcept;
void set_wait_recv_time(std::uint32_t value) noexcept;

} // namespace ch572::rfend
