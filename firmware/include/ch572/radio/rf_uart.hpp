#pragma once

#include <cstddef>
#include <cstdint>

#include "ch572/config.hpp"
#include "ch572/radio/rfip.hpp"
#include "ch572/usb/usb_device.hpp"
#include "ch572/util/ring_buffer.hpp"

namespace ch572::rf_uart {

inline constexpr std::size_t kRfBufferSize = 256;
inline constexpr std::size_t kMaxPayload = 64;
inline constexpr std::size_t kPacketHeaderSize = 2;
inline constexpr std::size_t kMaxRxPacket = kMaxPayload + kPacketHeaderSize;
inline constexpr std::size_t kRxBufferSize = (kMaxRxPacket + 3u) & ~std::size_t{3u};

inline constexpr std::uint16_t kConnectionInterval = 10;
inline constexpr std::uint16_t kRfOnUs = 20;

inline constexpr std::uint32_t kDefaultFrequency = 17;
inline constexpr std::uint32_t kDefaultAccessAddress = 0x57250425u;
inline constexpr std::uint32_t kDefaultCrcInit = 0x555555u;
inline constexpr std::uint32_t kDefaultCrcPoly = 0x80032Du;

// One request/response pair carries data in both directions. A request with
// length 0 is a poll; a response with length 0 is an ACK. The device echoes the
// request sequence, so a duplicate request simply retransmits the last
// response.
inline constexpr std::uint8_t kPacketRequest = 0;
inline constexpr std::uint8_t kPacketResponse = 1;

struct CH572_PACKED Packet
{
    std::uint8_t type : 3;
    std::uint8_t sequence : 5;
    std::uint8_t length;
};

struct alignas(4) TxBuffer
{
    Packet packet;
    std::uint8_t payload[kMaxPayload];
};

static_assert(sizeof(Packet) == 2, "RF packet header is two bytes");
static_assert(offsetof(TxBuffer, payload) == 2, "RF payload is contiguous");
static_assert(sizeof(TxBuffer) == 68, "RF transmit buffer is header plus payload");

class RfUart final : public usb::ByteSource
{
public:
    explicit RfUart(usb::UsbDevice &usb) noexcept : m_usb(usb) {}

    void init() noexcept;
    std::size_t read(std::uint8_t *dst, std::size_t max_length) noexcept override;

private:
    CH572_HIGH_CODE static void on_status(rf::RoleEvent event) noexcept;

    CH572_HIGH_CODE void process_rx(Packet *packet) noexcept;
    CH572_HIGH_CODE void process_tx() noexcept;
    CH572_HIGH_CODE void start_tx() noexcept;
    CH572_HIGH_CODE void start_rx() noexcept;

    usb::UsbDevice &m_usb;
    RingBuffer<kRfBufferSize> m_rf_buffer;
    TxBuffer m_tx_buffer;
    alignas(4) std::uint8_t m_rx_buffer[kRxBufferSize];

    rf::TxParams m_tx_params;
    rf::RxParams m_rx_params;
    rf::RoleConfig m_role_config;

    std::uint32_t m_interval_timer = static_cast<std::uint32_t>(kConnectionInterval) * 1000u;
    std::uint8_t m_last_sequence = 0xFFu;
    bool m_tx_in_flight = false;
};

} // namespace ch572::rf_uart
