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

// Flow-control water marks for the radio receive ring. While it is above the
// high mark the local frame advertises busy, so the peer stops adding payload;
// it is only cleared once the ring has drained below the low mark, which keeps
// the bit from toggling on every frame.
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

    // Earliest time the next frame may be started. One variable serves both
    // roles, since a device is only ever one of them. Master: paces requests
    // so the slave has re-armed its receiver after the previous ACK. Slave:
    // delays the ACK so the master has re-armed its receiver before it starts.
    volatile std::uint32_t      m_tx_after     = 0;

    // Master: deadline for the ACK of the outstanding request. When it passes,
    // the cached request is retransmitted unchanged.
    volatile std::uint32_t      m_ack_deadline = 0;

    // Sequence of the outstanding (master) or last accepted (slave) request.
    // One variable serves both roles, since a device is only ever one of them.
    // The master toggles it after an ACK; the slave compares it while active
    // to detect a retransmission and replay the ACK held in m_tx.
    volatile bool               m_seq          = false;

    // A radio turn is outstanding. One variable serves both roles: the master
    // sets it when it sends a request and clears it on the ACK, which keeps a
    // second request off the air; the slave sets it when a reply is queued and
    // clears it in poll() just before transmitting that ACK.
    volatile bool               m_turn_pending = false;

    // Line coding shared by both roles. Master: the latest host setting, sent
    // in the next Reset while m_coding_pending; written from the USB interrupt
    // and consumed in poll(). Slave: the coding received in the last Reset,
    // applied by poll() right before its ACK. A pending slave is inactive: no
    // serial bytes move and a data request is answered with Reset. It stays
    // pending from power-on until a Reset carries a coding.
    serial::LineCoding          m_coding{115200u, serial::kStopBits1, serial::kParityNone, 8};
    volatile bool               m_coding_pending = true;

    // Slave: the pending reply still has to take its payload from the UART ring.
    // The read runs in poll() with the transmitter, not in the radio interrupt,
    // so a 240-byte copy cannot extend the interrupt-off window that the UART
    // receiver FIFO depends on. Cleared once the payload has been built, so a
    // duplicate request replays the same bytes instead of consuming new ones.
    volatile bool               m_reply_needs_data = false;

    // Flow control. m_local_busy is our receive ring above kRfHighWater and is
    // advertised in every outgoing frame; m_peer_busy is the last valid value
    // received from the peer and suppresses the payload we would otherwise add
    // to the next frame.
    volatile bool               m_local_busy = false;
    volatile bool               m_peer_busy  = false;
};

} // namespace ch572::rf_uart
