#include "ch572/radio/rf_uart.hpp"

#include "ch572/platform/registers.hpp"

namespace ch572::rf_uart
{
namespace
{

RfUart* g_instance = nullptr;

} // namespace

void RfUart::init() noexcept
{
    g_instance = this;

    rf::init(&RfUart::on_status);


    m_waiting = false;

    if (m_role == Role::master)
    {
        m_seq      = 0;
        m_deadline = systick::now();
    }
    else
    {
        // The slave treats 0xFF as "no request seen yet".
        m_seq = 0xFF;
        start_rx();
    }
}

void RfUart::poll() noexcept
{
    if (m_role != Role::master)
    {
        return;
    }

    // poll() runs in the main loop while the radio state is also touched by the
    // RF interrupt, so make the check-and-start indivisible.
    CriticalSection guard;

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

    auto&             out    = m_tx.packet;
    const std::size_t length = m_usb.read_host(m_tx.payload, rf::kMaxPayload);

    out.type     = kPacketRequest;
    out.sequence = m_seq;
    out.length   = static_cast<std::uint8_t>(length);

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
    rf::start_tx(&m_tx);
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
    // a stale or foreign frame, so keep listening until the deadline.
    if (packet->type != kPacketResponse || !m_waiting || packet->sequence != m_seq)
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
    if (packet->type != kPacketRequest)
    {
        if (!m_tx_in_flight)
        {
            start_rx();
        }
        return;
    }

    auto& out = m_tx.packet;
    if (packet->sequence != m_seq)
    {
        m_seq = packet->sequence;
        if (packet->length != 0 && packet->length <= rf::kMaxPayload)
        {
            m_rf_buffer.write(packet + 1, packet->length);
        }

        const std::size_t length = m_usb.read_host(m_tx.payload, rf::kMaxPayload);
        out.type                 = kPacketResponse;
        out.sequence             = packet->sequence;
        out.length               = static_cast<std::uint8_t>(length);
    }

    start_tx();
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
