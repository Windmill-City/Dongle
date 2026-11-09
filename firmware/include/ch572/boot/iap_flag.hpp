#pragma once

namespace ch572::iap {

bool is_iap_requested() noexcept;
void mark_app_valid() noexcept;
void jump_to_iap() noexcept;

} // namespace ch572::iap
