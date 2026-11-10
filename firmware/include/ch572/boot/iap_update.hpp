#pragma once

#include <cstdint>

#include "ch572/usb/usb_device.hpp"

namespace ch572::iap {

// Serve the framed upgrade protocol over the CDC ACM data endpoint until the
// host completes the update. The caller chooses the writable flash window:
// the application uses [kIapStart, kIapEnd), the bootloader uses
// [kAppStart, kAppEnd). On success the chip is reset.
void update_region(usb::UsbDevice &usb, std::uint32_t start, std::uint32_t end) noexcept;

} // namespace ch572::iap
