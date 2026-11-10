#pragma once

#include <cstdint>

#include "ch572/usb/usb_device.hpp"

namespace ch572::iap {

// Serve the framed upgrade protocol over the CDC ACM data endpoint until the
// host completes the update. On success the chip is reset.
[[noreturn]] void update_region(usb::UsbDevice &usb, std::uint32_t start,
                                std::uint32_t end) noexcept;

// Receive a new boot image into the staging area, verify it, copy it to
// 0x0000 from RAM, and reset.
[[noreturn]] void update_boot(usb::UsbDevice &usb) noexcept;

} // namespace ch572::iap
