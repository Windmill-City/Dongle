#pragma once

#include <cstdint>

namespace ch572::hal {

// Diagnostic build only: select the blink group that reports the init stage.
void set_led_stage(std::uint32_t stage) noexcept;

} // namespace ch572::hal
