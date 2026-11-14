#include "ch572/radio/rf_uart.hpp"

#include <cstring>

#include "ch572/platform/registers.hpp"
#include "ch572/serial/uart.hpp"

namespace ch572::rf_uart
{
namespace
{

RfUart*         g_instance = nullptr;

// The master's local side is the USB CDC port; the slave's is the UART that
// shares the USB-A pads.
CH572_HIGH_CODE std::size_t
                read_local(usb::UsbDevice& usb, Role role, std::uint8_t* dst, std::size_t max_length) noexcept
{
    return role == Role::master ? usb.read_host(dst, max_length) : serial::read(dst, max_length);
}

} // namespace

void RfUart::init() noexcept
{
    g_instance = this;

    rf::init(&RfUart::on_status);

    m_waiting = false;

    if (m_role == Role::master)
    {
        m_seq                 = 0;
        m_deadline            = systick::now();

        // Prime the slave with the host's line coding on the first exchange.
        m_line_coding_pending = true;
    }
    else
    {
        // The slave treats 0xFF as "no request seen yet".
        m_seq = 0xFF;
        start_rx();
    }
}

void RfUart::set_line_coding(const serial::LineCoding& coding) noexcept
{
    if (m_role != Role::master)
    {
        return;
    }

    m_line_coding         = coding;
    m_line_coding_pending = true;
}

void RfUart::poll() noexcept
{
    if (m_role != Role::master)
    {
        // Forward radio payloads to the local serial port; the UART interrupt
        // buffers the other direction.
        std::uint8_t buffer[rf::kMaxPayload];
        std::size_t  length = 0;
        while ((length = m_rf_buffer.read(buffer, sizeof(buffer))) != 0)
        {
            serial::write(buffer, length);
        }

        // Recover from a missed TX-done so a lost interrupt cannot wedge the
        // response path.
        if (m_tx_in_flight && systick::expired(m_tx_deadline))
        {
            CriticalSection guard;
            m_tx_in_flight = false;
            start_rx();
        }

        // Answer a received request from the main loop, not from the RX
        // interrupt, and hold off briefly so the master has already re-armed
        // RX by the time this response is transmitted.
        if (m_tx_pending && !m_tx_in_flight && systick::expired(m_tx_after))
        {
            CriticalSection guard;
            m_tx_pending = false;
            start_tx();
        }
        return;
    }

    // poll() runs in the main loop while the radio state is also touched by the
    // RF interrupt, so make the check-and-start indivisible.
    CriticalSection guard;

    if (m_tx_in_flight && systick::expired(m_tx_deadline))
    {
        // TX-done was lost (CRC abort, missed interrupt). Resume the normal
        // state machine instead of waiting forever.
        m_tx_in_flight = false;
    }

    if (m_waiting)
    {
        // Wait for TX-done before judging the response deadline; the request
        // must not be aborted while it is still on the air.
        if (m_tx_in_flight || !systick::expired(m_deadline))
        {
            return;
        }

        // No response in time: retransmit the cached request. The sequence is
        // kept, so a slave that already answered only replays its response.
        m_deadline = systick::now() + kResponseTimeoutTicks;
        start_tx();
        return;
    }

    if (!systick::expired(m_deadline))
    {
        return;
    }

    auto& out = m_tx.packet;

    if (m_line_coding_pending)
    {
        // A line-coding request replaces one data poll. It is sent with the
        // current sequence, so the normal retry path keeps repeating it until
        // the slave answers.
        std::memcpy(m_tx.payload, &m_line_coding, sizeof(serial::LineCoding));
        m_line_coding_pending = false;

        out.command           = kCommandLineCoding;
        out.sequence          = m_seq;
        out.length            = static_cast<std::uint8_t>(sizeof(serial::LineCoding));
    }
    else
    {
        const std::size_t length = read_local(m_usb, m_role, m_tx.payload, rf::kMaxPayload);

        out.command              = kCommandData;
        out.sequence             = m_seq;
        out.length               = static_cast<std::uint8_t>(length);
    }

    m_waiting  = true;
    m_deadline = systick::now() + kResponseTimeoutTicks;
    start_tx();
}

void RfUart::on_status(rf::Event event) noexcept
{
    if (g_instance == nullptr)
    {
        return;
    }

    auto* self = g_instance;
    if ((event & rf::kEventEr) != 0 && !self->m_tx_in_flight)
    {
        self->start_rx();
    }
    if ((event & rf::kEventTx) != 0)
    {
        self->process_tx();
    }
    if ((event & rf::kEventRx) != 0)
    {
        if (self->m_role == Role::master)
        {
            self->process_master_rsp(&self->m_rx.packet);
        }
        else
        {
            self->process_slave_req(&self->m_rx.packet);
        }
    }
}

void RfUart::start_tx() noexcept
{
    m_tx_in_flight = true;
    m_tx_deadline  = systick::now() + kTxWatchdogTicks;
    rf::set_tx(&m_tx);
}

void RfUart::start_rx() noexcept
{
    // Listen until a frame arrives or the next TX stops the receiver. The
    // software deadline handles master retries, so no hardware window is used.
    rf::set_rx(&m_rx, 0);
}

void RfUart::process_master_rsp(rf::Packet* packet) noexcept
{
    // Only the response to the outstanding request is useful. Anything else is
    // a stale frame, so keep listening until the deadline.
    if (!m_waiting || packet->sequence != m_seq)
    {
        if (!m_tx_in_flight)
        {
            start_rx();
        }
        return;
    }

    if (packet->length != 0 && packet->length <= rf::kMaxPayload)
    {
        m_rf_buffer.write(packet + 1, packet->length);
    }

    m_waiting  = false;
    m_seq      = static_cast<std::uint8_t>((m_seq + 1u) & kSequenceMask);
    m_deadline = systick::now() + kPollIntervalTicks;
}

void RfUart::process_slave_req(rf::Packet* packet) noexcept
{
    auto& out = m_tx.packet;
    if (packet->sequence != m_seq)
    {
        m_seq = packet->sequence;

        if (packet->command == kCommandLineCoding)
        {
            if (static_cast<std::size_t>(packet->length) == sizeof(serial::LineCoding))
            {
                serial::LineCoding coding;
                std::memcpy(&coding, packet + 1, sizeof(coding));
                serial::configure(coding);
            }
        }
        else if (packet->length != 0 && packet->length <= rf::kMaxPayload)
        {
            m_rf_buffer.write(packet + 1, packet->length);
        }

        const std::size_t length = read_local(m_usb, m_role, m_tx.payload, rf::kMaxPayload);
            out.command = packet->command;
        out.sequence = packet->sequence;
        out.length   = static_cast<std::uint8_t>(length);
    }

    m_tx_after   = systick::now() + kSlaveResponseDelayTicks;
    m_tx_pending = true;
}

void RfUart::process_tx() noexcept
{
    m_tx_in_flight = false;
    if (m_role == Role::master && m_waiting)
    {
        m_deadline = systick::now() + kResponseTimeoutTicks;
    }
    start_rx();
}

std::size_t RfUart::read(std::uint8_t* dst, std::size_t max_length) noexcept
{
    return m_rf_buffer.read(dst, max_length);
}

} // namespace ch572::rf_uart
