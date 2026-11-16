#pragma once

#include <cstddef>
#include <cstdint>

#include "ch572/compiler.hpp"

namespace ch572::rf
{

using Event                                       = std::uint32_t;
using ProcessCallback                             = void (*)(Event event);

// Events reported to the role callback. These are deliberately independent
// from the raw LLE IRQ bits, which are decoded inside rfip.cpp.
inline constexpr Event         kEventTx           = 1u << 0;
inline constexpr Event         kEventRx           = 1u << 1;

inline constexpr std::uint8_t  kTxPowerMinus25Dbm = 0x01;
inline constexpr std::uint8_t  kTxPowerMinus20Dbm = 0x02;
inline constexpr std::uint8_t  kTxPowerMinus15Dbm = 0x03;
inline constexpr std::uint8_t  kTxPowerMinus10Dbm = 0x05;
inline constexpr std::uint8_t  kTxPowerMinus8Dbm  = 0x07;
inline constexpr std::uint8_t  kTxPowerMinus5Dbm  = 0x0A;
inline constexpr std::uint8_t  kTxPowerMinus3Dbm  = 0x0C;
inline constexpr std::uint8_t  kTxPowerMinus1Dbm  = 0x10;
inline constexpr std::uint8_t  kTxPower0Dbm       = 0x12;
inline constexpr std::uint8_t  kTxPower1Dbm       = 0x15;
inline constexpr std::uint8_t  kTxPower2Dbm       = 0x18;
inline constexpr std::uint8_t  kTxPower3Dbm       = 0x1B;
inline constexpr std::uint8_t  kTxPower4Dbm       = 0x1F;
inline constexpr std::uint8_t  kTxPower5Dbm       = 0x25;
inline constexpr std::uint8_t  kTxPower6Dbm       = 0x2D;

// Fixed link configuration for the transparent UART bridge.
inline constexpr std::uint32_t kLinkFrequency     = 17;
inline constexpr std::uint32_t kLinkAccessAddress = 0x57250425u;
inline constexpr std::uint32_t kLinkCrcInit       = 0x555555u;
inline constexpr std::uint32_t kLinkCrcPoly       = 0x80032Du;
inline constexpr std::uint8_t  kLinkTxPower       = kTxPower0Dbm;
inline constexpr std::uint8_t  kRfOnUs            = 80;
inline constexpr std::uint8_t  kLinkWaitTime      = static_cast<std::uint8_t>(kRfOnUs * 2u);

// Packet header at the DMA base. The hardware takes the payload length from
// the second byte of the first word, so `length` must stay at offset 1.
// Byte 0 carries the six-bit command in the low bits, one flow-control bit and
// the alternating one-bit sequence. Direction is implicit: only the master
// sends requests and only the slave sends responses.
//
// The flow-control bit means "my receive buffer is above its high-water mark":
// the peer must answer with an empty payload (length 0) until a later frame
// clears it, so a full ring can never silently discard a whole frame.
struct CH572_PACKED Packet
{
    std::uint8_t command : 6;
    std::uint8_t busy    : 1;
    std::uint8_t sequence: 1;
    std::uint8_t length;
};

static_assert(sizeof(Packet) == 2, "RF packet header is two bytes");

inline constexpr std::size_t  kMaxPayload = 240;
inline constexpr std::uint8_t kMaxPacket  = static_cast<std::uint8_t>(kMaxPayload + sizeof(Packet));

// One DMA frame: packet header followed by the payload. TX and RX share the
// layout; on RX only bytes up to packet.length are valid. The radio writes the
// header, payload and its trailing CRC/status bytes straight into this buffer,
// so it is sized like the vendor RxBuf. Receiving directly into the frame keeps
// the interrupt path free of payload copies.
inline constexpr std::size_t kDmaFrameSize = 264;

struct alignas(4) Frame
{
    Packet       packet;
    std::uint8_t payload[kDmaFrameSize - sizeof(Packet)];
};

static_assert(offsetof(Frame, payload) == sizeof(Packet), "RF payload is contiguous");
static_assert(sizeof(Frame) >= sizeof(Packet) + kMaxPayload, "RF frame holds header plus payload");
static_assert(sizeof(Frame) == kDmaFrameSize, "RF frame matches the receive DMA footprint");

void                 init(ProcessCallback process_cb) noexcept;

CH572_HIGH_CODE void set_tx(Frame* frame) noexcept;
CH572_HIGH_CODE void set_rx(Frame* frame) noexcept;

CH572_HIGH_CODE void lle_irq_handler() noexcept;
CH572_HIGH_CODE void bb_irq_handler() noexcept;




} // namespace ch572::rf
