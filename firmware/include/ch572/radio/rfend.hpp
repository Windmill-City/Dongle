#pragma once

#include <cstdint>

namespace ch572::rfend
{

namespace detail
{
inline std::uint8_t g_tune_filter_2m = 0;
}

inline std::uint8_t tune_filter_2m() noexcept
{
    return detail::g_tune_filter_2m;
}

void init() noexcept;
void calibration() noexcept;

}
