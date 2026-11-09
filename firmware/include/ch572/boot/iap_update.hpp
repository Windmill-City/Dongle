#pragma once

#include "ch572/usb/ch341.hpp"

namespace ch572::iap {

void update_iap_region(usb::Ch341Serial &usb) noexcept;

} // namespace ch572::iap
