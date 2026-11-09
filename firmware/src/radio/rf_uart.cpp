#include "ch572/radio/rf_uart.hpp"

#include "ch572/platform/registers.hpp"

namespace ch572::rf_uart {
namespace {

RfUart *g_instance = nullptr;

CH572_HIGH_CODE std::uint32_t random16(std::uint32_t seed) noexcept
{
    static std::uint32_t state = 0;
    state += seed;
    state = state * 1664525u + 1013904223u;
    return state >> 16;
}

CH572_HIGH_CODE std::uint32_t random_access_address(std::uint16_t value) noexcept
{
    return (static_cast<std::uint32_t>(value) << 8) | 0x6E0000B6u;
}

} // namespace

void RfUart::init() noexcept
{
    g_instance = this;
    m_data_pending = false;
    m_data_sequence = 0;
    m_server_data = 0;
    m_link = LinkState::unbound;
    m_interval_timer = static_cast<std::uint32_t>(kConnectionInterval) * 1000u;
    m_radio = RadioState::suspended;

    m_tx_params.crc_init = kDefaultCrcInit;
    m_tx_params.tx_power = static_cast<std::int8_t>(rf::kTxPower0Dbm);
    m_tx_params.crc_poly = kDefaultCrcPoly;
    m_tx_params.wait_time = 0;

    m_rx_params.crc_init = kDefaultCrcInit;
    m_rx_params.crc_poly = kDefaultCrcPoly;
    m_rx_params.rx_dma = reinterpret_cast<std::uint32_t>(&m_rx_buffer[0]);
    m_rx_params.rx_max_len = static_cast<std::uint8_t>(kMaxRxPacket);
    m_rx_params.time_out = 0;

    configure_link(kDefaultAccessAddress, kDefaultFrequency);

    m_role_config.process_cb = &RfUart::on_status;
    rf::role_init(&m_role_config);

    m_radio = RadioState::receiving;
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
    if ((event & rf::kEventCrcError) != 0)
    {
        self->process_crc_error();
    }
    if ((event & rf::kEventTxFinish) != 0)
    {
        self->process_tx();
    }
    if ((event & rf::kEventTimeout) != 0)
    {
        self->count_timeout();
    }
}

void RfUart::start_tx() noexcept
{
    m_tx_params.tx_dma = reinterpret_cast<std::uint32_t>(&m_tx_buffer);
    m_tx_params.wait_time = static_cast<std::uint8_t>(kRfOnUs * 2u);
    rf::start_tx(&m_tx_params);
}

void RfUart::start_rx() noexcept
{
    m_rx_params.time_out = m_interval_timer * 2u;
    rf::set_rx(&m_rx_params);
}

void RfUart::configure_link(std::uint32_t access_address,
                            std::uint32_t frequency) noexcept
{
    m_tx_params.access_address = access_address;
    m_tx_params.frequency = frequency;

    m_rx_params.access_address = access_address;
    m_rx_params.frequency = frequency;
}

void RfUart::count_timeout() noexcept
{
    if (m_link != LinkState::unbound && ++m_timeout > m_timeout_max)
    {
        m_link = LinkState::lost;
    }
    if (m_radio == RadioState::receiving)
    {
        m_radio = RadioState::suspended;
    }
}

void RfUart::disconnect() noexcept
{
    rf::shutdown();
    m_link = LinkState::unbound;
    m_data_pending = false;
    m_timeout = 0;
    configure_link(kDefaultAccessAddress, kDefaultFrequency);
    m_radio = RadioState::receiving;
    start_rx();
}

void RfUart::apply_binding(const BoundResponse &response) noexcept
{
    m_timeout = 0;
    m_link = LinkState::awaiting;
    configure_link(response.access_address, response.channel);
}

void RfUart::process_rx(Packet *packet) noexcept
{
    auto &packet_out = m_tx_buffer.packet;
    auto &response = m_tx_buffer.response;

    if (m_link == LinkState::unbound)
    {
        if (packet->type == kPacketBoundRequest)
        {
            auto *request = reinterpret_cast<BoundRequest *>(packet + 1);
            if (packet->length == sizeof(BoundRequest))
            {
                const std::int8_t rssi = *reinterpret_cast<std::uint8_t *>(
                    reinterpret_cast<std::uint8_t *>(packet) + packet->length + kPacketHeaderSize);

                bool accept = false;
                if (request->server_data != 0)
                {
                    if (m_server_data == 0 || request->server_data == m_server_data)
                    {
                        accept = true;
                    }
                }
                else if (rssi > -35)
                {
                    accept = true;
                }

                if (accept)
                {
                    m_radio = RadioState::binding;
                    m_data_sequence = 0;
                    m_server_data = random16(static_cast<std::uint32_t>(rssi));

                    auto *binding = reinterpret_cast<BoundResponse *>(&response);
                    packet_out.type = kPacketBoundResponse;
                    packet_out.length = static_cast<std::uint8_t>(sizeof(BoundResponse));
                    packet_out.sequence = m_data_sequence;

                    binding->access_address = random_access_address(static_cast<std::uint16_t>(m_server_data));
                    binding->channel = static_cast<std::uint8_t>(m_server_data & 0x3Fu);
                    binding->phy = kConnectionPhy;
                    binding->server_data = static_cast<std::uint16_t>(m_server_data);
                    binding->interval = kConnectionInterval;
                    binding->timeout = kConnectionTimeout;

                    start_tx();
                    m_data_sequence = static_cast<std::uint8_t>((m_data_sequence + 1u) & kPacketSequenceMask);
                    m_interval_timer = static_cast<std::uint32_t>(request->interval) * 1000u;
                    m_timeout_max = kBindEstablishCount;
                    return;
                }
            }
        }

        m_radio = RadioState::suspended;
        return;
    }

    const bool first_request = (m_link == LinkState::awaiting);

    if (packet->type == kPacketGetStatus)
    {
        m_radio = RadioState::transmitting;
        if (packet->sequence == m_data_sequence)
        {
            packet_out.type = kPacketStatusResponse;
            if (first_request)
            {
                packet_out.length = static_cast<std::uint8_t>(
                    1 + sizeof(usb::LineCoding));
                response.opcode = kOpcodeBsp;
                response.uart = m_usb.line_coding();
                packet_out.sequence = m_data_sequence;
            }
            else
            {
                const auto result = m_usb.read_host(response.payload, kMaxPayload);
                if (result.status == usb::HostReadStatus::data)
                {
                    packet_out.length = static_cast<std::uint8_t>(
                        1 + result.length);
                    response.opcode = kOpcodeData;
                    packet_out.sequence = m_data_sequence;
                }
                else if (result.status == usb::HostReadStatus::line_coding_changed)
                {
                    packet_out.length = static_cast<std::uint8_t>(
                        1 + sizeof(usb::LineCoding));
                    response.opcode = kOpcodeBsp;
                    response.uart = m_usb.line_coding();
                    packet_out.sequence = m_data_sequence;
                }
                else
                {
                    packet_out.length = 0;
                    response.opcode = kOpcodeAck;
                    packet_out.sequence = m_data_sequence;
                }
            }
            m_data_sequence = static_cast<std::uint8_t>((m_data_sequence + 1u) & kPacketSequenceMask);
        }
        start_tx();
    }
    else if (packet->type == kPacketData)
    {
        m_radio = RadioState::transmitting;
        if (packet->sequence == m_data_sequence)
        {
            const std::size_t length = packet->length;
            if (length > 0)
            {
                m_rf_buffer.write(packet + 1, length);
                m_data_pending = true;
            }

            auto result = usb::HostReadResult{usb::HostReadStatus::no_data, 0};
            if (!first_request)
            {
                result = m_usb.read_host(response.payload, kMaxPayload);
            }

            packet_out.type = kPacketDataAck;
            if (result.status == usb::HostReadStatus::data)
            {
                packet_out.length = static_cast<std::uint8_t>(
                    1 + result.length);
                response.opcode = kOpcodeData;
            }
            else
            {
                packet_out.length = 0;
            }
            packet_out.sequence = m_data_sequence;
            m_data_sequence = static_cast<std::uint8_t>((m_data_sequence + 1u) & kPacketSequenceMask);
        }
        start_tx();
    }
    else
    {
        count_timeout();
        return;
    }

    if (first_request)
    {
        m_link = LinkState::connected;
        m_interval_timer = static_cast<std::uint32_t>(kConnectionInterval) * 1000u;
        m_timeout_max = static_cast<std::uint16_t>(
            (kConnectionTimeout * 10u) / kConnectionInterval);
    }
    m_timeout = 0;
}

void RfUart::process_tx() noexcept
{
    if (m_radio == RadioState::binding)
    {
        apply_binding(*reinterpret_cast<BoundResponse *>(&m_tx_buffer.response));
    }
    m_radio = RadioState::receiving;
    start_rx();
}

void RfUart::process_crc_error() noexcept
{
    m_radio = RadioState::suspended;
}

std::size_t RfUart::read(std::uint8_t *dst, std::size_t max_length) noexcept
{
    if (m_data_pending)
    {
        const std::size_t length = m_rf_buffer.read(dst, max_length);
        CriticalSection guard;
        if (m_rf_buffer.empty())
        {
            m_data_pending = false;
        }
        return length;
    }

    if (m_link == LinkState::lost)
    {
        disconnect();
        return 0;
    }

    if (m_radio == RadioState::suspended)
    {
        m_radio = RadioState::receiving;
        start_rx();
    }
    return 0;
}

} // namespace ch572::rf_uart
