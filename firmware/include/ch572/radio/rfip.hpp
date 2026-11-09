#pragma once

#include <cstddef>
#include <cstdint>

#include "ch572/config.hpp"

namespace ch572::rf {

using Status = std::uint8_t;
using RoleState = std::uint32_t;
using ProcessCallback = void (*)(RoleState state);

inline constexpr std::uint32_t kStateRx = 1u << 0;
inline constexpr std::uint32_t kStateTimeout = 1u << 2;
inline constexpr std::uint32_t kStateCrcError = 1u << 3;
inline constexpr std::uint32_t kStateTxFinish = 1u << 4;

inline constexpr std::uint32_t kPhyModeMask = 0x30;
inline constexpr std::uint32_t kPhyMode1M = 0u << 4;
inline constexpr std::uint32_t kPhyMode2M = 1u << 4;
inline constexpr std::uint32_t kPhyMode2G4 = 3u << 4;

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
    std::uint32_t process_mask;
};

// Layout matches the vendor rfipTx_t/rfipRx_t structures.  Keep the
// static_asserts below in sync if this is ever changed.
struct TxParams
{
    std::uint32_t access_address;
    std::uint8_t access_address_ex;
    std::uint8_t reserved0[3];
    std::uint32_t crc_init;
    std::uint32_t frequency;
    std::uint32_t properties;
    std::uint32_t tx_dma;
    std::uint8_t white_channel;
    std::uint8_t tx_len;
    std::uint8_t wait_time;
    std::int8_t tx_power;
    std::uint32_t crc_poly;
};

struct RxParams
{
    std::uint32_t access_address;
    std::uint8_t access_address_ex;
    std::uint8_t reserved0[3];
    std::uint32_t crc_init;
    std::uint32_t frequency;
    std::uint32_t properties;
    std::uint32_t rx_dma;
    std::uint8_t white_channel;
    std::uint8_t rx_max_len;
    std::uint32_t time_out;
    std::uint32_t crc_poly;
};

static_assert(sizeof(TxParams) == 32, "TxParams must match the vendor ABI");
static_assert(sizeof(RxParams) == 36, "RxParams must match the vendor ABI");
static_assert(offsetof(TxParams, tx_dma) == 20, "vendor ABI");
static_assert(offsetof(TxParams, white_channel) == 24, "vendor ABI");
static_assert(offsetof(TxParams, tx_len) == 25, "vendor ABI");
static_assert(offsetof(TxParams, wait_time) == 26, "vendor ABI");
static_assert(offsetof(TxParams, tx_power) == 27, "vendor ABI");
static_assert(offsetof(TxParams, crc_poly) == 28, "vendor ABI");
static_assert(offsetof(RxParams, rx_dma) == 20, "vendor ABI");
static_assert(offsetof(RxParams, rx_max_len) == 25, "vendor ABI");
static_assert(offsetof(RxParams, time_out) == 28, "vendor ABI");
static_assert(offsetof(RxParams, crc_poly) == 32, "vendor ABI");

Status role_init(RoleConfig *config) noexcept;
CH572_HIGH_CODE Status start_tx(TxParams *params) noexcept;
CH572_HIGH_CODE Status set_rx(RxParams *params) noexcept;
Status stop() noexcept;
Status shutdown() noexcept;
void calibration() noexcept;
void wakeup_reg_init() noexcept;
CH572_HIGH_CODE std::uint8_t read_crc() noexcept;

CH572_HIGH_CODE void lle_irq_handler() noexcept;
CH572_HIGH_CODE void bb_irq_handler() noexcept;

} // namespace ch572::rf
