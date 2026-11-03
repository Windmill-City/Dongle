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
    const std::uint64_t wide = state;
    return static_cast<std::uint32_t>((wide * 0xFFFFu) >> 32);
}

CH572_HIGH_CODE std::uint32_t random_access_address(std::uint16_t value) noexcept
{
    return (static_cast<std::uint32_t>(value) << 8) | 0x6E0000B6u;
}

void set_phy(std::uint32_t &properties, std::uint8_t phy) noexcept
{
    properties = (properties & ~rf::kPhyModeMask) |
                 (static_cast<std::uint32_t>(phy & 0x3u) << 4);
}

void fill_line_coding(Response &response, const usb::LineCoding &coding) noexcept
{
    response.uart.baud_rate = coding.baud_rate;
    response.uart.stop_bits = coding.stop_bits;
    response.uart.parity = coding.parity;
    response.uart.data_bits = coding.data_bits;
    response.uart.io_status = coding.io_status;
}

} // namespace

void RfUart::init() noexcept
{
    g_instance = this;
    m_rx_status = RxDataStatus::start;
    m_data_sequence = 0;
    m_server_data = 0;
    m_bound_status = BoundStatus::idle;
    m_interval_timer = static_cast<std::uint32_t>(kConnectionInterval) * 1000u;
    m_radio_status = RadioStatus::wait;

    m_tx_params.access_address = kDefaultAccessAddress;
    m_tx_params.access_address_ex = kDefaultAccessAddressEx;
    m_tx_params.crc_init = kDefaultCrcInit;
    m_tx_params.frequency = kDefaultFrequency;
    m_tx_params.properties = rf::kPhyMode2M;
    m_tx_params.tx_power = static_cast<std::int8_t>(rf::kTxPower0Dbm);
    m_tx_params.crc_poly = kDefaultCrcPoly;
    m_tx_params.tx_len = 0;
    m_tx_params.white_channel = 0;
    m_tx_params.wait_time = 0;

    m_rx_params.access_address = kDefaultAccessAddress;
    m_rx_params.access_address_ex = kDefaultAccessAddressEx;
    m_rx_params.crc_init = kDefaultCrcInit;
    m_rx_params.frequency = kDefaultFrequency;
    m_rx_params.properties = rf::kPhyMode2M;
    m_rx_params.crc_poly = kDefaultCrcPoly;
    m_rx_params.rx_dma = reinterpret_cast<std::uint32_t>(&m_rx_buffer[0]);
    m_rx_params.rx_max_len = static_cast<std::uint8_t>(kMaxRxPacket);
    m_rx_params.white_channel = 0;
    m_rx_params.time_out = 0;

    m_role_config.process_cb = &RfUart::on_status;
    m_role_config.process_mask =
        rf::kStateRx | rf::kStateTimeout | rf::kStateCrcError | rf::kStateTxFinish;
    rf::role_init(&m_role_config);

    m_radio_status = RadioStatus::receive;
    start_rx(m_interval_timer);
}

void RfUart::on_status(rf::RoleState state, std::uint8_t id) noexcept
{
    (void)id;
    if (g_instance == nullptr)
    {
        return;
    }

    if ((state & rf::kStateRx) != 0)
    {
        on_rx(reinterpret_cast<Packet *>(&g_instance->m_rx_buffer[0]));
    }
    if ((state & rf::kStateCrcError) != 0)
    {
        on_crc_error();
    }
    if ((state & rf::kStateTxFinish) != 0)
    {
        on_tx();
    }
    if ((state & rf::kStateTimeout) != 0)
    {
        on_timeout();
    }
}

void RfUart::on_rx(Packet *packet) noexcept
{
    if (g_instance != nullptr)
    {
        g_instance->process_rx(packet);
    }
}

void RfUart::on_tx() noexcept
{
    if (g_instance != nullptr)
    {
        g_instance->process_tx();
    }
}

void RfUart::on_crc_error() noexcept
{
    if (g_instance != nullptr)
    {
        g_instance->process_crc_error();
    }
}

void RfUart::on_timeout() noexcept
{
    if (g_instance != nullptr)
    {
        g_instance->process_timeout();
    }
}

void RfUart::start_tx(std::uint16_t rfon_us) noexcept
{
    m_tx_params.tx_dma = reinterpret_cast<std::uint32_t>(&m_tx_buffer);
    m_tx_params.wait_time = static_cast<std::uint8_t>(rfon_us * 2u);
    rf::start_tx(&m_tx_params);
}

void RfUart::start_rx(std::uint32_t rx_us) noexcept
{
    m_rx_params.time_out = rx_us * 2u;
    rf::set_rx(&m_rx_params);
}

void RfUart::update_radio_defaults() noexcept
{
    m_tx_params.access_address = kDefaultAccessAddress;
    m_tx_params.frequency = kDefaultFrequency;
    m_rx_params.access_address = kDefaultAccessAddress;
    m_rx_params.frequency = kDefaultFrequency;
}

void RfUart::disconnect() noexcept
{
    rf::shutdown();
    m_bound_status = BoundStatus::idle;
    m_rx_status = RxDataStatus::start;
    update_radio_defaults();
    m_radio_status = RadioStatus::wait;
}

void RfUart::apply_binding(const BoundResponse &response) noexcept
{
    m_timeout = 0;
    m_bound_status = BoundStatus::waiting;

    m_tx_params.access_address = response.access_address;
    m_tx_params.frequency = response.channel;
    set_phy(m_tx_params.properties, response.phy);

    m_rx_params.access_address = response.access_address;
    m_rx_params.frequency = response.channel;
    set_phy(m_rx_params.properties, response.phy);
}

void RfUart::process_rx(Packet *packet) noexcept
{
    auto &packet_out = m_tx_buffer.packet;
    auto &response = m_tx_buffer.response;

    if (m_bound_status == BoundStatus::idle)
    {
        if (packet->type == kPacketCommandBoundRequest)
        {
            auto *request = reinterpret_cast<BoundRequest *>(packet + 1);
            if (packet->length == sizeof(BoundRequest) + kDataOffset)
            {
                const std::int8_t rssi = *reinterpret_cast<std::uint8_t *>(
                    reinterpret_cast<std::uint8_t *>(packet) + packet->length + 4u);

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
                    m_radio_status = RadioStatus::transmit_response;
                    m_data_sequence = 0;
                    m_server_data = random16(static_cast<std::uint32_t>(rssi));

                    auto *binding = reinterpret_cast<BoundResponse *>(&response);
                    packet_out.type = kPacketCommandBoundResponse;
                    packet_out.length = static_cast<std::uint8_t>(kDataOffset + sizeof(BoundResponse));
                    packet_out.sequence = m_data_sequence;
                    packet_out.reserved = 0;

                    binding->access_address = random_access_address(static_cast<std::uint16_t>(m_server_data));
                    binding->channel = static_cast<std::uint8_t>(m_server_data & 0x3Fu);
                    binding->phy = kConnectionPhy;
                    binding->server_data = static_cast<std::uint16_t>(m_server_data);
                    binding->interval = kConnectionInterval;
                    binding->timeout = kConnectionTimeout;

                    start_tx(20);
                    ++m_data_sequence;
                    m_interval_timer = static_cast<std::uint32_t>(request->interval) * 1000u;
                    m_timeout_max = kBindEstablishCount;
                    return;
                }
            }
        }

        m_radio_status = RadioStatus::wait;
        return;
    }

    if (packet->type == kPacketCommandGetStatus)
    {
        m_radio_status = RadioStatus::transmit;
        if (packet->sequence == m_data_sequence)
        {
            packet_out.type = kPacketCommandStatusResponse;
            if (m_bound_status < BoundStatus::established)
            {
                packet_out.length = static_cast<std::uint8_t>(
                    kDataOffset + 1 + sizeof(usb::LineCoding));
                response.opcode = kOpcodeBsp;
                fill_line_coding(response, m_usb.line_coding());
                packet_out.sequence = m_data_sequence;
                packet_out.reserved = 0;
            }
            else
            {
                const auto result = m_usb.read_host(response.payload, kMaxPayload);
                if (result.status == usb::HostReadStatus::data)
                {
                    packet_out.length = static_cast<std::uint8_t>(
                        kDataOffset + 1 + result.length);
                    response.opcode = kOpcodeData;
                    packet_out.sequence = m_data_sequence;
                    packet_out.reserved = 0;
                }
                else if (result.status == usb::HostReadStatus::line_coding_changed)
                {
                    packet_out.length = static_cast<std::uint8_t>(
                        kDataOffset + 1 + sizeof(usb::LineCoding));
                    response.opcode = kOpcodeBsp;
                    fill_line_coding(response, m_usb.line_coding());
                    packet_out.sequence = m_data_sequence;
                    packet_out.reserved = 0;
                }
                else
                {
                    packet_out.length = static_cast<std::uint8_t>(kDataOffset);
                    response.opcode = kOpcodeAck;
                    packet_out.sequence = m_data_sequence;
                    packet_out.reserved = 0;
                }
            }
            ++m_data_sequence;
        }
        start_tx(20);
    }
    else if (packet->type == kPacketDataFlag)
    {
        m_radio_status = RadioStatus::transmit;
        if (packet->sequence == m_data_sequence)
        {
            std::size_t length = packet->length;
            if (length > kDataOffset)
            {
                length -= kDataOffset;
                m_rf_buffer.write(packet + 1, length);
                m_rx_status = RxDataStatus::receiving;
            }

            auto result = usb::HostReadResult{usb::HostReadStatus::no_data, 0};
            if (m_bound_status == BoundStatus::established)
            {
                result = m_usb.read_host(response.payload, kMaxPayload);
            }

            packet_out.type = kPacketDataAck;
            if (result.status == usb::HostReadStatus::data)
            {
                packet_out.length = static_cast<std::uint8_t>(
                    kDataOffset + 1 + result.length);
                response.opcode = kOpcodeData;
            }
            else
            {
                packet_out.length = static_cast<std::uint8_t>(kDataOffset);
            }
            packet_out.sequence = m_data_sequence;
            packet_out.reserved = 0;
            ++m_data_sequence;
        }
        start_tx(20);
    }
    else
    {
        if (++m_timeout > m_timeout_max)
        {
            m_rx_status = RxDataStatus::timeout;
        }
        else
        {
            m_radio_status = RadioStatus::wait;
        }
        return;
    }

    if (m_bound_status == BoundStatus::waiting)
    {
        m_bound_status = BoundStatus::established;
        m_interval_timer = static_cast<std::uint32_t>(kConnectionInterval) * 1000u;
        m_timeout_max = static_cast<std::uint16_t>(
            (kConnectionTimeout * 10u) / kConnectionInterval);
    }
    m_timeout = 0;
}

void RfUart::process_tx() noexcept
{
    if (m_radio_status == RadioStatus::transmit_response)
    {
        apply_binding(*reinterpret_cast<BoundResponse *>(&m_tx_buffer.response));
    }
    m_radio_status = RadioStatus::receive;
    start_rx(m_interval_timer);
}

void RfUart::process_crc_error() noexcept
{
    m_radio_status = RadioStatus::wait;
}

void RfUart::process_timeout() noexcept
{
    if (m_bound_status != BoundStatus::idle && ++m_timeout > m_timeout_max)
    {
        m_rx_status = RxDataStatus::timeout;
    }
    else
    {
        m_radio_status = RadioStatus::wait;
    }
}

std::size_t RfUart::read(std::uint8_t *dst, std::size_t max_length) noexcept
{
    if (m_rx_status == RxDataStatus::receiving)
    {
        const std::size_t length = m_rf_buffer.read(dst, max_length);
        if (m_rf_buffer.empty())
        {
            CriticalSection guard;
            m_rx_status = RxDataStatus::start;
        }
        return length;
    }

    if (m_rx_status == RxDataStatus::timeout)
    {
        disconnect();
        return 0;
    }

    if (m_radio_status == RadioStatus::wait)
    {
        m_radio_status = RadioStatus::receive;
        start_rx(m_interval_timer);
    }
    return 0;
}

} // namespace ch572::rf_uart
