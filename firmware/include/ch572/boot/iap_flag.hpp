#pragma once

#include "ch572/config.hpp"

namespace ch572::iap {

// True when the flag still points at IAP.
bool requested() noexcept;

// True when the application asked the bootloader to update the boot region
// through the staging area.
bool update_requested() noexcept;

// Marks the application as having started successfully.
void app_ok() noexcept;

// Sets the flag to IAP without resetting. The bootloader uses this before
// handing over to the application, so a reset during application start-up
// returns to the bootloader instead of booting a corrupt image.
void request() noexcept;

// Asks the bootloader to receive a new boot image into staging, copy it to
// 0x0000, and reset.
void request_update() noexcept;

// Sets the IAP flag and resets the chip. The application uses this to enter
// the bootloader's application-upgrade mode.
void jump() noexcept;

// Called from the RAM copy routine after the new boot image is written. It
// must run from RAM because the boot flash no longer contains the old code.
CH572_HIGH_CODE void boot_ok() noexcept;

} // namespace ch572::iap
