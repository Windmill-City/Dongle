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
inline constexpr std::size_t kPacketHeaderSize = 4;
inline constexpr std::size_t kDataOffset = 2;
inline constexpr std::size_t kMaxRxPacket = kMaxPayload + kPacketHeaderSize;

inline constexpr std::uint16_t kConnectionInterval = 10;
inline constexpr std::uint16_t kConnectionTimeout = 100;
inline constexpr std::uint8_t kConnectionPhy = static_cast<std::uint8_t>(rf::kPhyMode2M >> 4);
inline constexpr std::uint16_t kBindEstablishCount = 6;

inline constexpr std::uint32_t kDefaultFrequency = 17;
inline constexpr std::uint32_t kDefaultAccessAddress = 0x57250425u;
inline constexpr std::uint8_t kDefaultAccessAddressEx = 0;
inline constexpr std::uint32_t kDefaultCrcInit = 0x555555u;
inline constexpr std::uint32_t kDefaultCrcPoly = 0x80032Du;

inline constexpr std::uint8_t kPacketCommandBoundRequest = 0x01;
inline constexpr std::uint8_t kPacketCommandGetStatus = 0x02;
inline constexpr std::uint8_t kPacketDataFlag = 0x7E;
inline constexpr std::uint8_t kPacketCommandBoundResponse = 0x80 | kPacketCommandBoundRequest;
inline constexpr std::uint8_t kPacketCommandStatusResponse = 0x80 | kPacketCommandGetStatus;
inline constexpr std::uint8_t kPacketDataAck = 0x80 | kPacketDataFlag;

inline constexpr std::uint8_t kOpcodeData = 0x00;
inline constexpr std::uint8_t kOpcodeBsp = 0x01;
inline constexpr std::uint8_t kOpcodeAck = 0xF0;

struct CH572_PACKED Packet
{
    std::uint8_t type;
    std::uint8_t length;
    std::uint8_t sequence;
    std::uint8_t reserved;
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

static_assert(sizeof(Packet) == 4, "RF packet header is four bytes");
static_assert(sizeof(BoundRequest) == 4, "bound request layout");
static_assert(sizeof(BoundResponse) == 12, "bound response layout");

enum class RxDataStatus : std::uint8_t
{
    start,
    receiving,
    timeout,
};

enum class BoundStatus : std::uint8_t
{
    idle,
    waiting,
    established,
};

enum class RadioStatus : std::uint8_t
{
    wait,
    receive,
    transmit,
    transmit_response,
};

class RfUart final : public usb::ByteSource
{
public:
    explicit RfUart(usb::Ch341Serial &usb) noexcept : m_usb(usb) {}

    void init() noexcept;
    std::size_t read(std::uint8_t *dst, std::size_t max_length) noexcept override;

private:
    CH572_HIGH_CODE static void on_status(rf::RoleState state, std::uint8_t id) noexcept;
    CH572_HIGH_CODE static void on_rx(Packet *packet) noexcept;
    CH572_HIGH_CODE static void on_tx() noexcept;
    CH572_HIGH_CODE static void on_crc_error() noexcept;
    CH572_HIGH_CODE static void on_timeout() noexcept;

    CH572_HIGH_CODE void process_rx(Packet *packet) noexcept;
    CH572_HIGH_CODE void process_tx() noexcept;
    CH572_HIGH_CODE void process_crc_error() noexcept;
    CH572_HIGH_CODE void process_timeout() noexcept;
    void disconnect() noexcept;
    CH572_HIGH_CODE void apply_binding(const BoundResponse &response) noexcept;

    CH572_HIGH_CODE void start_tx(std::uint16_t rfon_us) noexcept;
    CH572_HIGH_CODE void start_rx(std::uint32_t rx_us) noexcept;
    void update_radio_defaults() noexcept;

    usb::Ch341Serial &m_usb;
    RingBuffer<kRfBufferSize> m_rf_buffer;
    TxBuffer m_tx_buffer{};
    alignas(4) std::uint8_t m_rx_buffer[264]{};

    rf::TxParams m_tx_params{};
    rf::RxParams m_rx_params{};
    rf::RoleConfig m_role_config{};

    std::uint32_t m_server_data = 0;
    std::uint32_t m_interval_timer = static_cast<std::uint32_t>(kConnectionInterval) * 1000u;
    std::uint16_t m_timeout_max = 0;
    std::uint16_t m_timeout = 0;
    std::uint8_t m_data_sequence = 0;
    RadioStatus m_radio_status = RadioStatus::wait;
    BoundStatus m_bound_status = BoundStatus::idle;
    RxDataStatus m_rx_status = RxDataStatus::start;
};

} // namespace ch572::rf_uart
