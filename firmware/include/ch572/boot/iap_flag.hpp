#pragma once

namespace ch572::iap {

// True when the flag still points at IAP.
bool requested() noexcept;

// Marks the application as having started successfully.
void app_ok() noexcept;

// Sets the flag to IAP without resetting. The bootloader uses this before
// handing over to the application, so a reset during application start-up
// returns to ISP.
void request() noexcept;

// Sets the IAP flag and resets the chip.
void jump() noexcept;

} // namespace ch572::iap
