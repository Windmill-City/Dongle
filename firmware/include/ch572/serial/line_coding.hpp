#pragma once

#include <cstdint>

#include "ch572/compiler.hpp"

namespace ch572::serial
{

// CDC ACM line coding, byte-for-byte identical to the seven bytes exchanged in
// GET_LINE_CODING / SET_LINE_CODING. Keeping the USB layout on the air avoids
// any conversion on both ends.
inline constexpr std::uint8_t kStopBits1   = 0;
inline constexpr std::uint8_t kStopBits1_5 = 1;
inline constexpr std::uint8_t kStopBits2   = 2;

inline constexpr std::uint8_t kParityNone  = 0;
inline constexpr std::uint8_t kParityOdd   = 1;
inline constexpr std::uint8_t kParityEven  = 2;
inline constexpr std::uint8_t kParityMark  = 3;
inline constexpr std::uint8_t kParitySpace = 4;

struct CH572_PACKED LineCoding
{
    std::uint32_t baud_rate;
    std::uint8_t  stop_bits;
    std::uint8_t  parity;
    std::uint8_t  data_bits;
};

static_assert(sizeof(LineCoding) == 7, "line coding matches the CDC payload");

} // namespace ch572::serial
