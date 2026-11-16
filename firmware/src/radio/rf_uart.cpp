#include "ch572/radio/rf_uart.hpp"

#include <cstring>

#include "ch572/platform/registers.hpp"
#include "ch572/serial/uart.hpp"

namespace ch572::rf_uart
{

namespace
{

RfUart*         g_instance = nullptr;

// The radio buffer is written from the radio interrupt and read from the USB
// interrupt or the main loop, so a read must not be split by a write: the ring
// rejects a payload outright when its free-space check looks full.
std::size_t read_radio_buffer(RingBuffer<kRfBufferSize>& ring, std::uint8_t* dst, std::size_t max_length) noexcept
{
    CriticalSection guard;
    return ring.read(dst, max_length);
}

// Adapters for serial::HostOutput. Keeping them here means the USB device sees
// only the interface header, not this class.
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

} // namespace

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
        // The slave powers up in reset state: it must not move serial data
        // until the host has sent a Reset carrying the LineCoding.
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
    // Update the local flow-control state with hysteresis before any frame is
    // built, so a filling receive ring is advertised before the peer adds more.
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
        // poll() runs in the main loop while the radio state is also touched by
        // the RF interrupt, so make the check-and-start indivisible.
        CriticalSection guard;

        // A request stays outstanding until its ACK arrives. The peer can miss
        // the first transmission while its receiver settles, so retransmit the
        // held frame unchanged; the slave answers a duplicate sequence by
        // replaying the ACK it already sent rather than consuming data twice.
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

    // Answer a received request before moving any host->slave bytes: those are
    // already buffered in m_rf_buffer, so the acknowledgement no longer waits
    // for the UART transmitter. A coding is applied right before its ACK, so
    // nothing moves under the old setting; a pending state left by power-on has
    // no request behind it and keeps the slave inactive. The reply payload is
    // read here, not in the interrupt, so the radio ISR stays short enough for
    // the UART receiver FIFO at high line rates.
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

    // Feed the UART FIFO with only as much as it can take right now. A blocking
    // write here would delay the next acknowledgement past the master's ACK
    // timeout again. m_rf_buffer is the transmit queue; the busy bit throttles
    // the peer when it backs up.
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
        // Reset carries the host's LineCoding, so a line coding change is sent
        // as another Reset. The slave reconfigures its UART and resumes only
        // after acknowledging it.
        std::memcpy(m_tx.payload, &m_coding, sizeof(serial::LineCoding));
        m_coding_pending = false;

        out.command = kCommandReset;
        out.length  = static_cast<std::uint8_t>(sizeof(serial::LineCoding));
        return;
    }

    // The peer asked for empty frames: leave the host bytes queued in the USB
    // ring instead of pushing them into a receive ring that is already full.
    const std::size_t length =
        (!m_peer_busy && m_host.read != nullptr) ? m_host.read(m_host.context, m_tx.payload, rf::kMaxPayload)
                                                 : 0;

    out.command = kCommandData;
    out.length  = static_cast<std::uint8_t>(length);
}

void RfUart::master_process_response(rf::Packet* packet) noexcept
{
    // Only a response with the outstanding sequence is useful. A duplicate
    // carries an older sequence and is dropped; the receiver is re-armed while
    // the request is still outstanding.
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
        // The slave has no applied LineCoding and answered a data request with
        // Reset, so the next request must carry the coding. This re-sends the
        // coding the master already knows, bringing the slave back up without
        // a host-side action.
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
        // The ring is filled here (radio interrupt) and drained by the USB
        // interrupt; its free-space check must not straddle both contexts.
        CriticalSection guard;
        if (m_rf_buffer.write(packet + 1, packet->length) != packet->length)
        {
            // No room for the whole payload. Keep the turn outstanding and
            // force an immediate retransmit of the same request once the ring
            // has drained; acknowledging and advancing here would lose up to
            // 240 bytes with no way for the slave to resend them.
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

    // The request carries the master's receive-buffer state: when set, this
    // reply must not take new bytes out of the UART ring, and they stay queued
    // for a later turn.
    m_peer_busy = packet->busy;

    if (command == kCommandReset)
    {
        // Reset is idempotent and is honoured even when its sequence repeats,
        // so a host that restarted can always recover an already running
        // slave. It carries the host's LineCoding; a malformed payload still
        // resets the slave, which then re-applies the coding it already holds.
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
        // Rule 6: a duplicate request replays the ACK still held in m_tx
        // without consuming serial data. Only the flow-control bit is
        // refreshed; the payload still has to be the one the master missed.
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
            // The slave was reset and still has no applied LineCoding: answer
            // with Reset so the master knows to send the one carrying it.
            m_tx.packet.command = kCommandReset;
        }
        else
        {
            if (length != 0)
            {
                CriticalSection guard;
                if (m_rf_buffer.write(packet + 1, length) != length)
                {
                    // The host ring is full, so this request cannot be
                    // accepted. Leave the sequence unchanged and send no
                    // reply: the master retransmits the same request after its
                    // ACK timeout and the payload is retried instead of being
                    // silently dropped here.
                    return;
                }
            }

            // Built in poll(), after the radio interrupt has returned. Skipped
            // while the master advertises a full receive ring so its data stays
            // in our UART ring instead of being discarded on arrival.
            m_reply_needs_data = !m_peer_busy;
        }
    }

    // Rule 7: a new sequence consumes exactly one frame in each direction.
    // While inactive there is no valid last sequence, so each request is new
    // and is answered with Reset above.
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
    // A Reset must not interleave between clearing the pending coding and
    // configuring the UART, otherwise its new coding could be overwritten.
    CriticalSection guard;
    if (!m_coding_pending)
    {
        return;
    }

    const serial::LineCoding coding = m_coding;
    m_coding_pending                = false;

    serial::configure(coding);
    serial::resume();

    // Host->slave bytes buffered while the coding was pending belong to the old
    // configuration; drop them instead of forwarding them under the new one.
    std::uint8_t discard[32];
    while (read_radio_buffer(m_rf_buffer, discard, sizeof(discard)) != 0)
    {
    }
}

std::size_t RfUart::read(std::uint8_t* dst, std::size_t max_length) noexcept
{
    return read_radio_buffer(m_rf_buffer, dst, max_length);
}

} // namespace ch572::rf_uart
