#pragma once

#include "ch572/usb/ch341.hpp"

namespace ch572::iap {

// Receive a new IAP bootloader image over the USB serial link and program it
// into 0x00000000-0x00001FFF while the application runs from 0x2000.
//
// The bootloader region is only ever erased after the host sent the trigger
// magic and started the framed protocol, so a plain application session never
// touches it. This function does not return: on success it resets the chip so
// the new bootloader starts, and on protocol errors it keeps serving the host
// so the offending command can be retried. Once the erase succeeded the
// function must not reset early, because 0x0-0x1FFF no longer holds a complete
// bootloader.
void update_iap_region(usb::Ch341Serial &usb) noexcept;

} // namespace ch572::iap
