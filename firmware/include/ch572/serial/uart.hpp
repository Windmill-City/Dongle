#pragma once

#include <cstddef>
#include <cstdint>

#include "ch572/compiler.hpp"
#include "ch572/serial/line_coding.hpp"

namespace ch572::serial
{

// The slave rewires the USB-A pads as a 3.3 V UART: PA0 = TXD, PA1 = RXD.
// Received bytes are buffered from the UART interrupt so the radio side can
// take a whole request even though the hardware FIFO is only eight bytes.
inline constexpr std::uint32_t kBaudRate     = 115200;
inline constexpr std::size_t   kRxBufferSize = 512;

void                           init(std::uint32_t baud = kBaudRate) noexcept;

// Apply the host's line coding (baud rate, stop bits, parity, data bits) and
// drop anything that was received at the previous settings.
void                           configure(const LineCoding& coding) noexcept;

CH572_HIGH_CODE std::size_t read(std::uint8_t* dst, std::size_t max_length) noexcept;
void                        write(const std::uint8_t* src, std::size_t length) noexcept;

} // namespace ch572::serial
