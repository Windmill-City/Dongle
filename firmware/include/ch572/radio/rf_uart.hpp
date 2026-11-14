#pragma once

#include <cstddef>
#include <cstdint>

#include "ch572/compiler.hpp"
#include "ch572/platform/clock.hpp"
#include "ch572/radio/rfip.hpp"
#include "ch572/serial/line_coding.hpp"
#include "ch572/usb/usb_device.hpp"
#include "ch572/util/ring_buffer.hpp"

namespace ch572::rf_uart
{

inline constexpr std::size_t kRfBufferSize = 1024;

// The link has one master and one slave. The master polls with a REQ and the
// slave answers with one RSP; the REQ payload carries the master's local data
// and the RSP payload carries the slave's local data, so every poll moves both
// directions. The header's four-bit sub-command also lets a REQ carry control
// data instead of payload (see kCommand*).
enum class Role : std::uint8_t
{
    master,
    slave,
};

inline constexpr std::uint32_t kPollIntervalMs       = 6;
inline constexpr std::uint32_t kResponseTimeoutMs    = 5;
inline constexpr std::uint32_t kSlaveResponseDelayMs = 1;
inline constexpr std::uint32_t kTxWatchdogMs         = 15;
inline constexpr std::uint32_t kSequenceMask         = 0x0Fu;

inline constexpr std::uint32_t kPollIntervalTicks    = kPollIntervalMs * clock::kTicksPerMs;
inline constexpr std::uint32_t kResponseTimeoutTicks = kResponseTimeoutMs * clock::kTicksPerMs;
inline constexpr std::uint32_t kSlaveResponseDelayTicks = kSlaveResponseDelayMs * clock::kTicksPerMs;
inline constexpr std::uint32_t kTxWatchdogTicks      = kTxWatchdogMs * clock::kTicksPerMs;

inline constexpr std::uint8_t  kCommandData          = 0;
inline constexpr std::uint8_t  kCommandLineCoding    = 1;

class RfUart final
{
  public:
    RfUart(usb::UsbDevice& usb, Role role) noexcept
        : m_usb(usb)
        , m_role(role)
    {
    }

    void        init() noexcept;
    void        poll() noexcept;
    std::size_t read(std::uint8_t* dst, std::size_t max_length) noexcept;

    // Called by the USB device when the host changes the CDC line settings.
    // The next request carries them to the slave before any payload.
    void        set_line_coding(const serial::LineCoding& coding) noexcept;

  private:
    CH572_HIGH_CODE static void on_status(rf::Event event) noexcept;

    CH572_HIGH_CODE void        process_master_rsp(rf::Packet* packet) noexcept;
    CH572_HIGH_CODE void        process_slave_req(rf::Packet* packet) noexcept;
    CH572_HIGH_CODE void        process_tx() noexcept;
    CH572_HIGH_CODE void        start_tx() noexcept;
    CH572_HIGH_CODE void        start_rx() noexcept;

    usb::UsbDevice&             m_usb;
    RingBuffer<kRfBufferSize>   m_rf_buffer;

    rf::Frame                   m_tx;
    rf::Frame                   m_rx;

    Role                        m_role;
    volatile std::uint32_t      m_deadline     = 0;
    volatile std::uint8_t       m_seq          = 0;

    // Master: a request is outstanding, the slave owes a response.
    volatile bool               m_waiting      = false;
    volatile bool               m_tx_in_flight = false;

    // Slave: a response is queued. It is sent from poll() after a short delay
    // rather than from the RX interrupt, so the master has time to re-arm its
    // receiver before the response hits the air and the two sides can never
    // transmit in lockstep.
    volatile bool               m_tx_pending   = false;
    volatile std::uint32_t      m_tx_after     = 0;

    // Guards against a lost TX-done interrupt wedging the state machine.
    volatile std::uint32_t      m_tx_deadline  = 0;

    // Master: the latest host line coding and whether it still has to reach
    // the slave. Written from the USB interrupt, consumed in poll().
    serial::LineCoding          m_line_coding{115200u, serial::kStopBits1, serial::kParityNone, 8};
    volatile bool               m_line_coding_pending = false;
};

} // namespace ch572::rf_uart
