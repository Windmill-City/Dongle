#pragma once

#include <cstdint>

#include "ch572/compiler.hpp"

namespace ch572::rf {

using RoleEvent = std::uint32_t;
using ProcessCallback = void (*)(RoleEvent event);

// Events reported to the role callback. These are deliberately independent
// from the raw LLE IRQ bits, which are decoded inside rfip.cpp.
inline constexpr RoleEvent kEventRx = 1u << 0;
inline constexpr RoleEvent kEventCrcError = 1u << 1;
inline constexpr RoleEvent kEventTxFinish = 1u << 2;
inline constexpr RoleEvent kEventTimeout = 1u << 3;

inline constexpr std::uint8_t kTxPowerMinus25Dbm = 0x01;
inline constexpr std::uint8_t kTxPowerMinus20Dbm = 0x02;
inline constexpr std::uint8_t kTxPowerMinus15Dbm = 0x03;
inline constexpr std::uint8_t kTxPowerMinus10Dbm = 0x05;
inline constexpr std::uint8_t kTxPowerMinus8Dbm = 0x07;
inline constexpr std::uint8_t kTxPowerMinus5Dbm = 0x0A;
inline constexpr std::uint8_t kTxPowerMinus3Dbm = 0x0C;
inline constexpr std::uint8_t kTxPowerMinus1Dbm = 0x10;
inline constexpr std::uint8_t kTxPower0Dbm = 0x12;
inline constexpr std::uint8_t kTxPower1Dbm = 0x15;
inline constexpr std::uint8_t kTxPower2Dbm = 0x18;
inline constexpr std::uint8_t kTxPower3Dbm = 0x1B;
inline constexpr std::uint8_t kTxPower4Dbm = 0x1F;
inline constexpr std::uint8_t kTxPower5Dbm = 0x25;
inline constexpr std::uint8_t kTxPower6Dbm = 0x2D;

struct RoleConfig
{
    ProcessCallback process_cb;
};

// The link always uses the 2 Mbps PHY and a channel index (0..63), so the
// parameters only carry what the transmit and receive paths actually need.
struct TxParams
{
    std::uint32_t access_address;
    std::uint32_t crc_init;
    std::uint32_t frequency;
    std::uint32_t tx_dma;
    std::uint8_t wait_time;
    std::int8_t tx_power;
    std::uint32_t crc_poly;
};

struct RxParams
{
    std::uint32_t access_address;
    std::uint32_t crc_init;
    std::uint32_t frequency;
    std::uint32_t rx_dma;
    std::uint8_t rx_max_len;
    std::uint32_t time_out;
    std::uint32_t crc_poly;
};

void role_init(RoleConfig *config) noexcept;
CH572_HIGH_CODE void start_tx(TxParams *params) noexcept;
CH572_HIGH_CODE void set_rx(RxParams *params) noexcept;

CH572_HIGH_CODE void lle_irq_handler() noexcept;
CH572_HIGH_CODE void bb_irq_handler() noexcept;

} // namespace ch572::rf
