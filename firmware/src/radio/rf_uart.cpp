#include "ch572/radio/rf_uart.hpp"

namespace ch572::rf_uart {
namespace {

RfUart *g_instance = nullptr;

} // namespace

void RfUart::init() noexcept
{
    g_instance = this;
    m_last_sequence = 0xFFu;
    m_tx_in_flight = false;

    m_tx_params.crc_init = kDefaultCrcInit;
    m_tx_params.tx_power = static_cast<std::int8_t>(rf::kTxPower0Dbm);
    m_tx_params.crc_poly = kDefaultCrcPoly;
    m_tx_params.wait_time = 0;
    m_tx_params.access_address = kDefaultAccessAddress;
    m_tx_params.frequency = kDefaultFrequency;

    m_rx_params.crc_init = kDefaultCrcInit;
    m_rx_params.crc_poly = kDefaultCrcPoly;
    m_rx_params.access_address = kDefaultAccessAddress;
    m_rx_params.frequency = kDefaultFrequency;
    m_rx_params.rx_dma = reinterpret_cast<std::uint32_t>(&m_rx_buffer[0]);
    m_rx_params.rx_max_len = static_cast<std::uint8_t>(kMaxRxPacket);
    m_rx_params.time_out = 0;

    m_role_config.process_cb = &RfUart::on_status;
    rf::role_init(&m_role_config);

    start_rx();
}

void RfUart::on_status(rf::RoleEvent event) noexcept
{
    if (g_instance == nullptr)
    {
        return;
    }

    auto *self = g_instance;
    if ((event & rf::kEventRx) != 0)
    {
        self->process_rx(reinterpret_cast<Packet *>(&self->m_rx_buffer[0]));
    }
    if ((event & rf::kEventCrcError) != 0 && !self->m_tx_in_flight)
    {
        self->start_rx();
    }
    if ((event & rf::kEventTxFinish) != 0)
    {
        self->process_tx();
    }
    if ((event & rf::kEventTimeout) != 0 && !self->m_tx_in_flight)
    {
        self->start_rx();
    }
}

void RfUart::start_tx() noexcept
{
    m_tx_in_flight = true;
    m_tx_params.tx_dma = reinterpret_cast<std::uint32_t>(&m_tx_buffer);
    m_tx_params.wait_time = static_cast<std::uint8_t>(kRfOnUs * 2u);
    rf::start_tx(&m_tx_params);
}

void RfUart::start_rx() noexcept
{
    m_rx_params.time_out = m_interval_timer * 2u;
    rf::set_rx(&m_rx_params);
}

void RfUart::process_rx(Packet *packet) noexcept
{
    if (packet->type != kPacketRequest)
    {
        if (!m_tx_in_flight)
        {
            start_rx();
        }
        return;
    }

    auto &out = m_tx_buffer.packet;
    if (packet->sequence != m_last_sequence)
    {
        m_last_sequence = packet->sequence;
        if (packet->length != 0 && packet->length <= kMaxPayload)
        {
            m_rf_buffer.write(packet + 1, packet->length);
        }

        const std::size_t length = m_usb.read_host(m_tx_buffer.payload, kMaxPayload);
        out.type = kPacketResponse;
        out.sequence = packet->sequence;
        out.length = static_cast<std::uint8_t>(length);
    }

    start_tx();
}

void RfUart::process_tx() noexcept
{
    m_tx_in_flight = false;
    start_rx();
}

std::size_t RfUart::read(std::uint8_t *dst, std::size_t max_length) noexcept
{
    return m_rf_buffer.read(dst, max_length);
}

} // namespace ch572::rf_uart
