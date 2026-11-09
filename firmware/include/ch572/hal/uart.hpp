#pragma once

#include <cstdint>

namespace ch572::hal::uart {

void init(std::uint32_t baud) noexcept;
void putc(char c) noexcept;
void puts(const char *text) noexcept;
void put_u32(std::uint32_t value) noexcept;
void put_hex32(std::uint32_t value) noexcept;

} // namespace ch572::hal::uart
