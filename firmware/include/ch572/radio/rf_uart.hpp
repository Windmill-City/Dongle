#pragma once

#include <cstddef>
#include <cstdint>

#include "ch572/compiler.hpp"
#include "ch572/radio/rfip.hpp"
#include "ch572/usb/usb_device.hpp"
#include "ch572/util/ring_buffer.hpp"

namespace ch572::rf_uart
{

inline constexpr std::size_t   kRfBufferSize         = 256;

inline constexpr std::uint32_t kConnectionIntervalMs = 10 * 1000;

// One request/response pair carries data in both directions. A request with
// length 0 is a poll; a response with length 0 is an ACK. The device echoes the
// request sequence, so a duplicate request simply retransmits the last
// response.
inline constexpr std::uint8_t  kPacketRequest        = 0;
inline constexpr std::uint8_t  kPacketResponse       = 1;

class RfUart final
{
  public:
    explicit RfUart(usb::UsbDevice& usb) noexcept
        : m_usb(usb)
    {
    }

    void        init() noexcept;
    std::size_t read(std::uint8_t* dst, std::size_t max_length) noexcept;

  private:
    CH572_HIGH_CODE static void on_status(rf::Event event) noexcept;

    CH572_HIGH_CODE void        process_rx(rf::Packet* packet) noexcept;
    CH572_HIGH_CODE void        process_tx() noexcept;
    CH572_HIGH_CODE void        start_tx() noexcept;
    CH572_HIGH_CODE void        start_rx() noexcept;

    usb::UsbDevice&             m_usb;
    RingBuffer<kRfBufferSize>   m_rf_buffer;

    rf::Frame                   m_tx;
    rf::Frame                   m_rx;
    std::uint8_t                m_seq          = 0;

    bool                        m_tx_in_flight = false;
};

} // namespace ch572::rf_uart
