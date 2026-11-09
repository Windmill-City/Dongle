#pragma once

#include <cstdint>

#include "ch572/usb/ch341.hpp"

namespace ch572::iap {

// Serve the framed upgrade protocol over the CH341 serial endpoint until the
// host completes the update. The caller chooses the writable flash window:
// the application uses [kIapStart, kIapEnd), the bootloader uses
// [kAppStart, kAppEnd). On success the chip is reset.
void update_region(usb::Ch341Serial &usb, std::uint32_t start, std::uint32_t end) noexcept;

} // namespace ch572::iap
