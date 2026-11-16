#include "ch572/radio/rf_uart.hpp"

#include <cstring>

#include "ch572/platform/registers.hpp"
#include "ch572/serial/uart.hpp"

namespace ch572::rf_uart
{

namespace
{

RfUart*         g_instance = nullptr;

std::size_t read_radio_buffer(RingBuffer<kRfBufferSize>& ring, std::uint8_t* dst, std::size_t max_length) noexcept
{
    CriticalSection guard;
    return ring.read(dst, max_length);
}

std::size_t output_read(void* context, void* dst, std::size_t max_length) noexcept
{
    return static_cast<RfUart*>(context)->read(static_cast<std::uint8_t*>(dst), max_length);
}

void output_set_line_coding(void* context, const serial::LineCoding& coding) noexcept
{
    static_cast<RfUart*>(context)->set_coding(coding);
}

serial::LineCoding output_coding(void* context) noexcept
{
    return static_cast<RfUart*>(context)->coding();
}

}

void RfUart::init() noexcept
{
    g_instance = this;

    rf::init(&RfUart::on_rf_evt);

    if (m_role == Role::master)
    {
        m_tx_after = systick::now();
    }
    else
    {
        serial::suspend();
        rf::set_rx(&m_rx);
    }
}

void RfUart::attach(serial::HostInput host) noexcept
{
    m_host = host;
}

serial::HostOutput RfUart::output() noexcept
{
    return serial::HostOutput{this, &output_read, &output_set_line_coding, &output_coding};
}

void RfUart::set_coding(const serial::LineCoding& coding) noexcept
{
    CriticalSection guard;
    m_coding         = coding;
    m_coding_pending = true;
}

serial::LineCoding RfUart::coding() const noexcept
{
    CriticalSection guard;
    return m_coding;
}

void RfUart::poll() noexcept
{
    const std::size_t queued = m_rf_buffer.size();
    if (queued >= kRfHighWater)
    {
        m_local_busy = true;
    }
    else if (queued <= kRfLowWater)
    {
        m_local_busy = false;
    }

    if (m_role != Role::slave)
    {
        CriticalSection guard;

        if (m_turn_pending)
        {
            if (!systick::expired(m_ack_deadline))
            {
                return;
            }

            rf::set_tx(&m_tx);
            m_ack_deadline = systick::now() + kAckTimeoutTicks;
            return;
        }

        if (!systick::expired(m_tx_after))
        {
            return;
        }

        master_build_request();
        m_turn_pending = true;
        rf::set_tx(&m_tx);
        m_ack_deadline = systick::now() + kAckTimeoutTicks;
        return;
    }

    if (m_turn_pending && systick::expired(m_tx_after))
    {
        CriticalSection guard;
        if (m_coding_pending)
        {
            slave_apply_line_coding();
        }
        if (m_reply_needs_data)
        {
            m_tx.packet.length = static_cast<std::uint8_t>(serial::read(m_tx.payload, rf::kMaxPayload));
            m_reply_needs_data = false;
        }
        m_tx.packet.busy = m_local_busy;
        m_turn_pending   = false;
        rf::set_tx(&m_tx);
    }

    std::uint8_t buffer[8];
    std::size_t  space = serial::tx_space();
    while (space != 0)
    {
        const std::size_t chunk  = space < sizeof(buffer) ? space : sizeof(buffer);
        const std::size_t length = read_radio_buffer(m_rf_buffer, buffer, chunk);
        if (length == 0)
        {
            break;
        }
        if (!m_coding_pending)
        {
            serial::write(buffer, length);
            space -= length;
        }
    }
}

void RfUart::on_rf_evt(rf::Event event) noexcept
{
    if (g_instance == nullptr)
    {
        return;
    }

    auto* self = g_instance;
    if ((event & rf::kEventTx) != 0)
    {
        rf::set_rx(&self->m_rx);
    }
    if ((event & rf::kEventRx) != 0)
    {
        if (self->m_role == Role::master)
        {
            self->master_process_response(&self->m_rx.packet);
        }
        else
        {
            self->slave_process_request(&self->m_rx.packet);
        }
    }
}

void RfUart::master_build_request() noexcept
{
    auto& out    = m_tx.packet;
    out.sequence = m_seq;
    out.busy     = m_local_busy;

    if (m_coding_pending)
    {
        std::memcpy(m_tx.payload, &m_coding, sizeof(serial::LineCoding));
        m_coding_pending = false;

        out.command = kCommandReset;
        out.length  = static_cast<std::uint8_t>(sizeof(serial::LineCoding));
        return;
    }

    const std::size_t length =
        (!m_peer_busy && m_host.read != nullptr) ? m_host.read(m_host.context, m_tx.payload, rf::kMaxPayload)
                                                 : 0;

    out.command = kCommandData;
    out.length  = static_cast<std::uint8_t>(length);
}

void RfUart::master_process_response(rf::Packet* packet) noexcept
{
    if (!m_turn_pending || packet->sequence != m_seq)
    {
        rf::set_rx(&m_rx);
        return;
    }

    if (packet->length > rf::kMaxPayload)
    {
        rf::set_rx(&m_rx);
        return;
    }

    if (packet->command == kCommandReset && m_tx.packet.command != kCommandReset)
    {
        m_peer_busy      = packet->busy;
        m_turn_pending   = false;
        m_seq            = !m_seq;
        m_coding_pending = true;
        m_tx_after       = systick::now() + kPollIntervalTicks;
        return;
    }

    if (packet->command != m_tx.packet.command)
    {
        rf::set_rx(&m_rx);
        return;
    }

    m_peer_busy = packet->busy;

    if (m_tx.packet.command == kCommandData && packet->length != 0)
    {
        CriticalSection guard;
        if (m_rf_buffer.write(packet + 1, packet->length) != packet->length)
        {
            m_ack_deadline = systick::now();
            rf::set_rx(&m_rx);
            return;
        }
    }

    m_turn_pending = false;
    m_seq          = !m_seq;

    m_tx_after = systick::now() + kPollIntervalTicks;
}

void RfUart::slave_process_request(rf::Packet* packet) noexcept
{
    const std::uint8_t command  = packet->command;
    const bool         sequence = packet->sequence;
    const std::uint8_t length   = packet->length;

    m_peer_busy = packet->busy;

    if (command == kCommandReset)
    {
        m_coding_pending = true;
        serial::suspend();

        if (static_cast<std::size_t>(length) == sizeof(serial::LineCoding))
        {
            std::memcpy(&m_coding, packet + 1, sizeof(m_coding));
        }

        m_seq = sequence;

        m_tx.packet.command  = kCommandReset;
        m_tx.packet.sequence = sequence;
        m_tx.packet.length   = 0;
        m_tx.packet.busy     = m_local_busy;
        m_reply_needs_data   = false;
        slave_schedule_reply();
        return;
    }

    if (!m_coding_pending && sequence == m_seq)
    {
        m_tx.packet.busy = m_local_busy;
        slave_schedule_reply();
        return;
    }

    m_tx.packet.command  = command;
    m_tx.packet.sequence = sequence;
    m_tx.packet.length   = 0;
    m_tx.packet.busy     = m_local_busy;
    m_reply_needs_data   = false;

    if (command == kCommandData)
    {
        if (m_coding_pending)
        {
            m_tx.packet.command = kCommandReset;
        }
        else
        {
            if (length != 0)
            {
                CriticalSection guard;
                if (m_rf_buffer.write(packet + 1, length) != length)
                {
                    return;
                }
            }

            m_reply_needs_data = !m_peer_busy;
        }
    }

    m_seq = sequence;
    slave_schedule_reply();
}

CH572_HIGH_CODE void RfUart::slave_schedule_reply() noexcept
{
    m_tx_after     = systick::now() + kSlaveResponseDelayTicks;
    m_turn_pending = true;
}

void RfUart::slave_apply_line_coding() noexcept
{
    CriticalSection guard;
    if (!m_coding_pending)
    {
        return;
    }

    const serial::LineCoding coding = m_coding;
    m_coding_pending                = false;

    serial::configure(coding);
    serial::resume();

    std::uint8_t discard[32];
    while (read_radio_buffer(m_rf_buffer, discard, sizeof(discard)) != 0)
    {
    }
}

std::size_t RfUart::read(std::uint8_t* dst, std::size_t max_length) noexcept
{
    return read_radio_buffer(m_rf_buffer, dst, max_length);
}

}
