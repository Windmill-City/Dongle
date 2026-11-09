#pragma once

#include <cstdint>

namespace ch572::flash {

bool erase(std::uint32_t address, std::uint32_t length) noexcept;
bool write(std::uint32_t address, const void *buffer, std::uint32_t length) noexcept;
bool verify(std::uint32_t address, const void *buffer, std::uint32_t length) noexcept;

} // namespace ch572::flash
