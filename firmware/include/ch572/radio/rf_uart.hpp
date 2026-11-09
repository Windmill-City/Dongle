#pragma once

#include <cstddef>
#include <cstdint>

#include "ch572/config.hpp"
#include "ch572/radio/rfip.hpp"
#include "ch572/usb/ch341.hpp"
#include "ch572/util/ring_buffer.hpp"

namespace ch572::rf_uart {

inline constexpr std::size_t kRfBufferSize = 512;
inline constexpr std::size_t kMaxPayload = 251;
inline constexpr std::size_t kPacketHeaderSize = 2;
inline constexpr std::size_t kMaxRxPacket = kMaxPayload + kPacketHeaderSize;
inline constexpr std::size_t kRxBufferSize = (kMaxRxPacket + 3u) & ~std::size_t{3u};

inline constexpr std::uint16_t kConnectionInterval = 10;
inline constexpr std::uint16_t kConnectionTimeout = 100;
inline constexpr std::uint8_t kConnectionPhy = static_cast<std::uint8_t>(rf::kPhyMode2M >> 4);
inline constexpr std::uint16_t kBindEstablishCount = 6;
inline constexpr std::uint16_t kRfOnUs = 20;

inline constexpr std::uint32_t kDefaultFrequency = 17;
inline constexpr std::uint32_t kDefaultAccessAddress = 0x57250425u;
inline constexpr std::uint8_t kDefaultAccessAddressEx = 0;
inline constexpr std::uint32_t kDefaultCrcInit = 0x555555u;
inline constexpr std::uint32_t kDefaultCrcPoly = 0x80032Du;

inline constexpr std::uint8_t kPacketBoundRequest = 0x0;
inline constexpr std::uint8_t kPacketGetStatus = 0x1;
inline constexpr std::uint8_t kPacketData = 0x2;
inline constexpr std::uint8_t kPacketResponseFlag = 0x4;
inline constexpr std::uint8_t kPacketBoundResponse = kPacketResponseFlag | kPacketBoundRequest;
inline constexpr std::uint8_t kPacketStatusResponse = kPacketResponseFlag | kPacketGetStatus;
inline constexpr std::uint8_t kPacketDataAck = kPacketResponseFlag | kPacketData;

inline constexpr std::uint8_t kPacketSequenceMask = 0x1F;

inline constexpr std::uint8_t kOpcodeData = 0x00;
inline constexpr std::uint8_t kOpcodeBsp = 0x01;
inline constexpr std::uint8_t kOpcodeAck = 0xF0;

struct CH572_PACKED Packet
{
    std::uint8_t type : 3;
    std::uint8_t sequence : 5;
    std::uint8_t length;
};

struct CH572_PACKED BoundRequest
{
    std::uint16_t interval;
    std::uint16_t server_data;
};

struct CH572_PACKED BoundResponse
{
    std::uint32_t access_address;
    std::uint8_t channel;
    std::uint8_t phy;
    std::uint16_t server_data;
    std::uint16_t interval;
    std::uint16_t timeout;
};

struct CH572_PACKED Response
{
    std::uint8_t opcode;
    union
    {
        struct
        {
            std::uint32_t baud_rate;
            std::uint8_t stop_bits;
            std::uint8_t parity;
            std::uint8_t data_bits;
            std::uint8_t io_status;
        } uart;
        std::uint8_t payload[kMaxPayload];
    };
};

struct TxBuffer
{
    Packet packet;
    Response response;
};

static_assert(sizeof(Packet) == 2, "RF packet header is two bytes");
static_assert(sizeof(BoundRequest) == 4, "bound request layout");
static_assert(sizeof(BoundResponse) == 12, "bound response layout");

// Link phase and radio phase are independent: the radio can be re-armed after
// backpressure without losing whether the link is unbound, awaiting, or up.
//
//   link:  unbound --BOUND_REQ accepted--> (radio binding) --TX done--> awaiting
//          awaiting --first valid request--> connected
//          awaiting/connected --missed packets--> lost --read()--> unbound
//
//   radio: receiving --packet--> transmitting/binding --TX done--> receiving
//          receiving --crc error/missed packet/backpressure--> suspended
//          suspended --read()--> receiving
enum class LinkState : std::uint8_t
{
    unbound,   // accepting BOUND_REQ
    awaiting,  // BOUND_RSP sent, waiting for the first valid request
    connected, // established
    lost,      // timeout; the next read() reinitialises the link
};

enum class RadioState : std::uint8_t
{
    receiving,    // RX armed
    transmitting, // response TX in flight
    binding,      // BOUND_RSP TX in flight; apply the binding on completion
    suspended,    // RX disarmed until read() resumes polling
};

class RfUart final : public usb::ByteSource
{
public:
    explicit RfUart(usb::Ch341Serial &usb) noexcept : m_usb(usb) {}

    void init() noexcept;
    std::size_t read(std::uint8_t *dst, std::size_t max_length) noexcept override;

private:
    CH572_HIGH_CODE static void on_status(rf::RoleEvent event) noexcept;

    CH572_HIGH_CODE void process_rx(Packet *packet) noexcept;
    CH572_HIGH_CODE void process_tx() noexcept;
    CH572_HIGH_CODE void process_crc_error() noexcept;
    CH572_HIGH_CODE void count_timeout() noexcept;
    void disconnect() noexcept;
    CH572_HIGH_CODE void apply_binding(const BoundResponse &response) noexcept;

    CH572_HIGH_CODE void start_tx() noexcept;
    CH572_HIGH_CODE void start_rx() noexcept;
    void configure_link(std::uint32_t access_address, std::uint32_t frequency,
                        std::uint8_t phy) noexcept;

    usb::Ch341Serial &m_usb;
    RingBuffer<kRfBufferSize> m_rf_buffer;
    TxBuffer m_tx_buffer{};
    alignas(4) std::uint8_t m_rx_buffer[kRxBufferSize]{};

    rf::TxParams m_tx_params{};
    rf::RxParams m_rx_params{};
    rf::RoleConfig m_role_config{};

    std::uint32_t m_server_data = 0;
    std::uint32_t m_interval_timer = static_cast<std::uint32_t>(kConnectionInterval) * 1000u;
    std::uint16_t m_timeout_max = 0;
    std::uint16_t m_timeout = 0;
    std::uint8_t m_data_sequence = 0;
    RadioState m_radio = RadioState::suspended;
    LinkState m_link = LinkState::unbound;
    bool m_data_pending = false;
};

} // namespace ch572::rf_uart
