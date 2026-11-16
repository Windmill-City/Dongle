#pragma once

#include <cstddef>
#include <cstdint>

#include "ch572/compiler.hpp"
#include "ch572/platform/clock.hpp"
#include "ch572/radio/rfip.hpp"
#include "ch572/serial/line_coding.hpp"
#include "ch572/serial/uart.hpp"
#include "ch572/util/ring_buffer.hpp"

namespace ch572::rf_uart
{

inline constexpr std::size_t kRfBufferSize = 2048;
inline constexpr std::size_t kRfHighWater = (kRfBufferSize * 3) / 4;
inline constexpr std::size_t kRfLowWater  = kRfBufferSize / 4;

enum class Role : std::uint8_t
{
    master,
    slave,
};

inline constexpr std::uint32_t kPollIntervalMs          = 6;
inline constexpr std::uint32_t kSlaveResponseDelayMs    = 1;
inline constexpr std::uint32_t kAckTimeoutMs            = 4;

inline constexpr std::uint32_t kPollIntervalTicks       = kPollIntervalMs * clock::kTicksPerMs;
inline constexpr std::uint32_t kSlaveResponseDelayTicks = kSlaveResponseDelayMs * clock::kTicksPerMs;
inline constexpr std::uint32_t kAckTimeoutTicks         = kAckTimeoutMs * clock::kTicksPerMs;

inline constexpr std::uint8_t  kCommandReset            = 0;
inline constexpr std::uint8_t  kCommandData             = 1;

class RfUart final
{
  public:
    explicit RfUart(Role role) noexcept
        : m_role(role)
    {
    }

    void               init() noexcept;
    void               poll() noexcept;
    std::size_t        read(std::uint8_t* dst, std::size_t max_length) noexcept;
    void               set_coding(const serial::LineCoding& coding) noexcept;

    void               attach(serial::HostInput host) noexcept;
    serial::HostOutput output() noexcept;
    serial::LineCoding coding() const noexcept;

  private:
    CH572_HIGH_CODE static void on_rf_evt(rf::Event event) noexcept;

    CH572_HIGH_CODE void        master_process_response(rf::Packet* packet) noexcept;
    void                        master_build_request() noexcept;

    CH572_HIGH_CODE void        slave_process_request(rf::Packet* packet) noexcept;
    CH572_HIGH_CODE void        slave_schedule_reply() noexcept;
    void                        slave_apply_line_coding() noexcept;

    serial::HostInput           m_host;
    RingBuffer<kRfBufferSize>   m_rf_buffer;

    rf::Frame                   m_tx;
    rf::Frame                   m_rx;

    Role                        m_role;

    volatile std::uint32_t      m_tx_after     = 0;
    volatile std::uint32_t      m_ack_deadline = 0;

    volatile bool               m_seq          = false;
    volatile bool               m_turn_pending = false;
    volatile bool               m_reply_needs_data = false;

    serial::LineCoding          m_coding{115200u, serial::kStopBits1, serial::kParityNone, 8};
    volatile bool               m_coding_pending = true;


    volatile bool               m_local_busy = false;
    volatile bool               m_peer_busy  = false;
};

}
