#include "ch572/radio/rf_uart.hpp"

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

    start_rx();
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
        self->process_rx(&self->m_rx.packet);
    }
}

void RfUart::start_tx() noexcept
{
    m_tx_in_flight = true;
    rf::start_tx(&m_tx);
}

void RfUart::start_rx() noexcept
{
    rf::set_rx(&m_rx, static_cast<std::uint32_t>(kConnectionIntervalMs * 2u));
}

void RfUart::process_rx(rf::Packet* packet) noexcept
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
    start_rx();
}

std::size_t RfUart::read(std::uint8_t* dst, std::size_t max_length) noexcept
{
    return m_rf_buffer.read(dst, max_length);
}

} // namespace ch572::rf_uart
