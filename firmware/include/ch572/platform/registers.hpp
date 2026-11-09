#pragma once

#include <cstddef>
#include <cstdint>

namespace ch572 {

inline void fence_i() noexcept { asm volatile("fence.i" ::: "memory"); }
inline void nop() noexcept { asm volatile("nop"); }

// The WCH part keeps a small set of system registers behind a two-byte unlock
// sequence.  Interrupts must be masked while the sequence is active.
inline std::uint32_t irq_save() noexcept
{
    std::uint32_t state;
    asm volatile("csrrc %0, 0x800, %1" : "=r"(state) : "r"(0x88u) : "memory");
    return state & 0x88u;
}

inline void irq_restore(std::uint32_t state) noexcept
{
    asm volatile("csrw 0x800, %0" : : "r"(state) : "memory");
}

class CriticalSection
{
public:
    CriticalSection() noexcept : m_state(irq_save()) {}
    ~CriticalSection() noexcept { irq_restore(m_state); }
    CriticalSection(const CriticalSection &) = delete;
    CriticalSection &operator=(const CriticalSection &) = delete;

private:
    std::uint32_t m_state;
};

// ---------------------------------------------------------------------------
// Interrupt controller and core timer
// ---------------------------------------------------------------------------

enum class Irq : std::uint32_t
{
    SysTick = 12,
    GpioA = 17,
    Spi = 19,
    BleBb = 20,
    BleLle = 21,
    Usb = 22,
    Timer = 24,
    Uart = 27,
    Rtc = 28,
    Comparator = 29,
    I2c = 30,
    Pwm = 31,
    KeyScan = 33,
    Encoder = 34,
    WatchdogBattery = 35,
};

struct Pfic
{
    volatile std::uint32_t isr[8];
    volatile std::uint32_t ipr[8];
    volatile std::uint32_t ithresdr;
    std::uint32_t reserved0[2];
    volatile std::uint32_t gisr;
    volatile std::uint8_t vtfidr[4];
    std::uint8_t reserved1[0x0C];
    volatile std::uint32_t vtfaddr[4];
    std::uint8_t reserved2[0x90];
    volatile std::uint32_t ienr[8];
    std::uint8_t reserved3[0x60];
    volatile std::uint32_t irer[8];
    std::uint8_t reserved4[0x60];
    volatile std::uint32_t ipsr[8];
    std::uint8_t reserved5[0x60];
    volatile std::uint32_t iprr[8];
    std::uint8_t reserved6[0x60];
    volatile std::uint32_t iactr[8];
    std::uint8_t reserved7[0xE0];
    volatile std::uint8_t iprior[256];
    std::uint8_t reserved8[0x810];
    volatile std::uint32_t sctlr;
};

struct SysTickRegisters
{
    volatile std::uint32_t ctlr;
    volatile std::uint32_t sr;
    volatile std::uint32_t cntl;
    std::uint32_t reserved0;
    volatile std::uint32_t cmpl;
};

inline Pfic &pfic() noexcept
{
    return *reinterpret_cast<Pfic *>(0xE000E000u);
}

inline SysTickRegisters &systick_regs() noexcept
{
    return *reinterpret_cast<SysTickRegisters *>(0xE000F000u);
}

inline void enable_irq(Irq irq) noexcept
{
    const auto index = static_cast<std::uint32_t>(irq) >> 5;
    const auto bit = 1u << (static_cast<std::uint32_t>(irq) & 0x1Fu);
    pfic().ienr[index] = bit;
}

// ---------------------------------------------------------------------------
// System control registers
// ---------------------------------------------------------------------------

namespace sysreg {

inline volatile std::uint8_t &safe_access_sig() noexcept
{
    return *reinterpret_cast<volatile std::uint8_t *>(0x40001040u);
}
inline volatile std::uint8_t &clock_sys_cfg() noexcept
{
    return *reinterpret_cast<volatile std::uint8_t *>(0x40001008u);
}
inline volatile std::uint8_t &hfck_power_ctrl() noexcept
{
    return *reinterpret_cast<volatile std::uint8_t *>(0x4000100Au);
}
inline volatile std::uint8_t &sleep_power_ctrl() noexcept
{
    return *reinterpret_cast<volatile std::uint8_t *>(0x4000100Fu);
}
inline volatile std::uint8_t &xt32m_tune() noexcept
{
    return *reinterpret_cast<volatile std::uint8_t *>(0x4000104Eu);
}
inline volatile std::uint8_t &flash_cfg() noexcept
{
    return *reinterpret_cast<volatile std::uint8_t *>(0x40001807u);
}
inline volatile std::uint8_t &flash_sck() noexcept
{
    return *reinterpret_cast<volatile std::uint8_t *>(0x40001805u);
}
inline volatile std::uint32_t &misc_ctrl() noexcept
{
    return *reinterpret_cast<volatile std::uint32_t *>(0x40001048u);
}
inline volatile std::uint8_t &sleep_clk_off1() noexcept
{
    return *reinterpret_cast<volatile std::uint8_t *>(0x4000100Du);
}
inline volatile std::uint8_t &sleep_clk_off2() noexcept
{
    return *reinterpret_cast<volatile std::uint8_t *>(0x4000101Du);
}
inline volatile std::uint8_t &reset_watchdog_ctrl() noexcept
{
    return *reinterpret_cast<volatile std::uint8_t *>(0x40001046u);
}
// R8_RESET_STATUS: bits 0..2 report the most recent reset source.
inline volatile std::uint8_t &reset_status() noexcept
{
    return *reinterpret_cast<volatile std::uint8_t *>(0x40001044u);
}
// Independent watchdog (IWDT): runs from the 32 kHz LSI, 4..512 prescaler,
// 12-bit reload.  timeout = prescaler * reload / 32768 s.
// The R8_WDOG_COUNT above counts at Fsys/131072 (8-bit), so at 100 MHz it can
// only reach ~0.33 s; use the IWDT whenever a longer timeout is needed.
inline volatile std::uint32_t &iwdg_key() noexcept
{
    return *reinterpret_cast<volatile std::uint32_t *>(0x40001000u);
}
inline volatile std::uint32_t &iwdg_cfg() noexcept
{
    return *reinterpret_cast<volatile std::uint32_t *>(0x40001004u);
}
inline void iwdg_feed() noexcept
{
    iwdg_key() = 0xAAAAu; // KEY_RELOADING_COUNT
}
inline void iwdg_start_5s() noexcept
{
    iwdg_key() = 0xCCCCu; // KEY_START_IWDG
    iwdg_key() = 0x5555u; // KEY_UNPROTECT (allow PR/RLR writes)
    // PR field bits 11..13: 4 -> /64.
    iwdg_cfg() = (iwdg_cfg() & ~(0x7u << 11)) | (4u << 11);
    for (std::uint32_t guard = 0; guard < 100000u && (iwdg_cfg() & (1u << 15)) != 0u; ++guard)
    {
    }
    // RLR field bits 16..27: 64 * 2560 / 32768 = 5.0 s.
    iwdg_cfg() = (iwdg_cfg() & ~(0xFFFu << 16)) | (2560u << 16);
    for (std::uint32_t guard = 0; guard < 100000u && (iwdg_cfg() & (1u << 15)) != 0u; ++guard)
    {
    }
    iwdg_key() = 0xAAAAu; // load the new timeout
}
inline volatile std::uint16_t &pin_alternate() noexcept
{
    return *reinterpret_cast<volatile std::uint16_t *>(0x40001018u);
}
inline constexpr std::uint8_t kSafeAccessSig1 = 0x57;
inline constexpr std::uint8_t kSafeAccessSig2 = 0xA8;
inline constexpr std::uint8_t kClockPllOn = 0x10;
inline constexpr std::uint8_t kClockXt32mOn = 0x04;
inline constexpr std::uint16_t kPinDebugEnable = 0x4000;
inline constexpr std::uint16_t kPinUsbEnable = 0x2000;
inline constexpr std::uint16_t kUdpPullUpEnable = 0x1000;
inline constexpr std::uint8_t kSoftwareReset = 0x01;
inline constexpr std::uint8_t kResetFlagMask = 0x07;
inline constexpr std::uint8_t kResetFlagSoftware = 0x00;
inline constexpr std::uint8_t kResetFlagPowerOn = 0x01;
inline constexpr std::uint8_t kResetFlagWatchdog = 0x02;
inline constexpr std::uint8_t kResetFlagManual = 0x03;

class SafeAccess
{
public:
    SafeAccess() noexcept : m_state(irq_save())
    {
        fence_i();
        safe_access_sig() = kSafeAccessSig1;
        safe_access_sig() = kSafeAccessSig2;
        fence_i();
    }

    ~SafeAccess() noexcept
    {
        safe_access_sig() = 0;
        irq_restore(m_state);
        fence_i();
    }

    SafeAccess(const SafeAccess &) = delete;
    SafeAccess &operator=(const SafeAccess &) = delete;

private:
    std::uint32_t m_state;
};

// Clear the watchdog reset enables left set by the reset defaults.
inline void disable_watchdog_reset() noexcept
{
    SafeAccess access;
    reset_watchdog_ctrl() &= ~0x06u;
}

} // namespace sysreg

// ---------------------------------------------------------------------------
// Flash ROM controller (serial command interface)
// ---------------------------------------------------------------------------

namespace flashreg {

inline volatile std::uint32_t &data32() noexcept
{
    return *reinterpret_cast<volatile std::uint32_t *>(0x40001800u);
}
inline volatile std::uint8_t &data8() noexcept
{
    return *reinterpret_cast<volatile std::uint8_t *>(0x40001804u);
}
inline volatile std::uint8_t &ctrl() noexcept
{
    return *reinterpret_cast<volatile std::uint8_t *>(0x40001806u);
}
inline volatile std::uint8_t &glob_rom_cfg() noexcept
{
    return *reinterpret_cast<volatile std::uint8_t *>(0x40001044u);
}

// R8_FLASH_CTRL
inline constexpr std::uint8_t kEn = 0x01;
inline constexpr std::uint8_t kReadAction = 0x04;
inline constexpr std::uint8_t kShift = 0x10;
inline constexpr std::uint8_t kAct = 0x80;

// R8_GLOB_ROM_CFG
inline constexpr std::uint8_t kRomCodeWe = 0xC0;
inline constexpr std::uint8_t kRomCtrlEn = 0x20;
inline constexpr std::uint8_t kRomCodeOfs = 0x10;

} // namespace flashreg

// ---------------------------------------------------------------------------
// UART (only the line-control register is read for CH341 compatibility)
// ---------------------------------------------------------------------------

namespace uartreg {

inline volatile std::uint8_t &lcr() noexcept
{
    return *reinterpret_cast<volatile std::uint8_t *>(0x40003403u);
}

} // namespace uartreg

// ---------------------------------------------------------------------------
// GPIO
// ---------------------------------------------------------------------------

namespace gpio {

inline volatile std::uint32_t &dir_reg() noexcept
{
    return *reinterpret_cast<volatile std::uint32_t *>(0x400010A0u);
}
inline volatile std::uint32_t &clear_reg() noexcept
{
    return *reinterpret_cast<volatile std::uint32_t *>(0x400010ACu);
}
inline volatile std::uint32_t &pull_up_reg() noexcept
{
    return *reinterpret_cast<volatile std::uint32_t *>(0x400010B0u);
}
inline volatile std::uint32_t &pull_down_drive_reg() noexcept
{
    return *reinterpret_cast<volatile std::uint32_t *>(0x400010B4u);
}
inline volatile std::uint32_t &set_reg() noexcept
{
    return *reinterpret_cast<volatile std::uint32_t *>(0x400010B8u);
}

} // namespace gpio

// ---------------------------------------------------------------------------
// USB device controller
// ---------------------------------------------------------------------------

namespace usbreg {

inline constexpr std::uintptr_t kBase = 0x40008000u;

template <typename T>
inline volatile T &at(std::size_t offset) noexcept
{
    return *reinterpret_cast<volatile T *>(kBase + offset);
}

inline volatile std::uint8_t &ctrl() noexcept { return at<std::uint8_t>(0x00); }
inline volatile std::uint8_t &dev_ctrl() noexcept { return at<std::uint8_t>(0x01); }
inline volatile std::uint8_t &int_en() noexcept { return at<std::uint8_t>(0x02); }
inline volatile std::uint8_t &dev_addr() noexcept { return at<std::uint8_t>(0x03); }
inline volatile std::uint8_t &int_fg() noexcept { return at<std::uint8_t>(0x06); }
inline volatile std::uint8_t &int_st() noexcept { return at<std::uint8_t>(0x07); }
inline volatile std::uint8_t &rx_len() noexcept { return at<std::uint8_t>(0x08); }
inline volatile std::uint8_t &ep4_1_mod() noexcept { return at<std::uint8_t>(0x0C); }
inline volatile std::uint8_t &ep2_3_mod() noexcept { return at<std::uint8_t>(0x0D); }
inline volatile std::uint16_t &ep0_dma() noexcept { return at<std::uint16_t>(0x10); }
inline volatile std::uint16_t &ep1_dma() noexcept { return at<std::uint16_t>(0x14); }
inline volatile std::uint16_t &ep2_dma() noexcept { return at<std::uint16_t>(0x18); }
inline volatile std::uint8_t &ep0_t_len() noexcept { return at<std::uint8_t>(0x20); }
inline volatile std::uint8_t &ep0_ctrl() noexcept { return at<std::uint8_t>(0x22); }
inline volatile std::uint8_t &ep1_ctrl() noexcept { return at<std::uint8_t>(0x26); }
inline volatile std::uint8_t &ep2_t_len() noexcept { return at<std::uint8_t>(0x28); }
inline volatile std::uint8_t &ep2_ctrl() noexcept { return at<std::uint8_t>(0x2A); }

inline constexpr std::uint8_t kUcDevPuEn = 0x20;
inline constexpr std::uint8_t kUcIntBusy = 0x08;
inline constexpr std::uint8_t kUcDmaEn = 0x01;
inline constexpr std::uint8_t kUdPdDis = 0x80;
inline constexpr std::uint8_t kUdPortEn = 0x01;
inline constexpr std::uint8_t kUieSuspend = 0x04;
inline constexpr std::uint8_t kUieTransfer = 0x02;
inline constexpr std::uint8_t kUieBusReset = 0x01;
inline constexpr std::uint8_t kUifTransfer = 0x02;
inline constexpr std::uint8_t kUifBusReset = 0x01;
inline constexpr std::uint8_t kUifSuspend = 0x04;
inline constexpr std::uint8_t kUisSetupAct = 0x80;
inline constexpr std::uint8_t kUisTogOk = 0x40;
inline constexpr std::uint8_t kMaskUisToken = 0x30;
inline constexpr std::uint8_t kMaskUisEndp = 0x0F;
inline constexpr std::uint8_t kUisTokenOut = 0x00;
inline constexpr std::uint8_t kUisTokenIn = 0x20;
inline constexpr std::uint8_t kUifNak = 0x80;
inline constexpr std::uint8_t kUcResetSie = 0x04;
inline constexpr std::uint8_t kUdaGpBit = 0x80;
inline constexpr std::uint8_t kUepAutoTog = 0x10;
inline constexpr std::uint8_t kUep1RxEn = 0x80;
inline constexpr std::uint8_t kUep1TxEn = 0x40;
inline constexpr std::uint8_t kUep2RxEn = 0x08;
inline constexpr std::uint8_t kUep2TxEn = 0x04;
inline constexpr std::uint8_t kUepRTog = 0x80;
inline constexpr std::uint8_t kUepTTog = 0x40;
inline constexpr std::uint8_t kMaskUepRRes = 0x0C;
inline constexpr std::uint8_t kUepRResAck = 0x00;
inline constexpr std::uint8_t kUepRResNak = 0x08;
inline constexpr std::uint8_t kUepRResStall = 0x0C;
inline constexpr std::uint8_t kMaskUepTRes = 0x03;
inline constexpr std::uint8_t kUepTResAck = 0x00;
inline constexpr std::uint8_t kUepTResNak = 0x02;
inline constexpr std::uint8_t kUepTResStall = 0x03;

} // namespace usbreg

// ---------------------------------------------------------------------------
// 2.4 GHz radio register windows
// ---------------------------------------------------------------------------

namespace radio_reg {

inline constexpr std::uintptr_t kLleBase = 0x4000C200u;
inline constexpr std::uintptr_t kBbBase = 0x4000C100u;
inline constexpr std::uintptr_t kRfEndBase = 0x4000D000u;

template <std::uintptr_t Base>
struct Window
{
    static volatile std::uint32_t &r32(std::size_t offset) noexcept
    {
        return *reinterpret_cast<volatile std::uint32_t *>(Base + offset);
    }
    static volatile std::uint16_t &r16(std::size_t offset) noexcept
    {
        return *reinterpret_cast<volatile std::uint16_t *>(Base + offset);
    }
    static volatile std::uint8_t &r8(std::size_t offset) noexcept
    {
        return *reinterpret_cast<volatile std::uint8_t *>(Base + offset);
    }
};

using Lle = Window<kLleBase>;
using Bb = Window<kBbBase>;
using RfEnd = Window<kRfEndBase>;

namespace lle {
inline constexpr std::size_t Control = 0x00;
inline constexpr std::size_t Config = 0x04;
inline constexpr std::size_t WaitTime = 0x06;
inline constexpr std::size_t Irq = 0x08;
inline constexpr std::size_t Config2 = 0x0C;
inline constexpr std::size_t PhyControl = 0x50;
inline constexpr std::size_t Timer = 0x64;
inline constexpr std::size_t TxDma = 0x78;
inline constexpr std::size_t Timeout = 0x7C;
inline constexpr std::size_t Config54 = 0x54;
} // namespace lle

namespace bb {
inline constexpr std::size_t Control = 0x00;
inline constexpr std::size_t CrcInit = 0x04;
inline constexpr std::size_t CrcPoly = 0x08;
inline constexpr std::size_t AccessAddress = 0x0C;
inline constexpr std::size_t AccessAddressEx = 0x10;
inline constexpr std::size_t Control1 = 0x14;
inline constexpr std::size_t Control2High = 0x1E;
inline constexpr std::size_t Control2 = 0x1C;
inline constexpr std::size_t Control3 = 0x24;
inline constexpr std::size_t Mode2G4 = 0x26;
inline constexpr std::size_t TxPower = 0x34;
inline constexpr std::size_t RxConfig = 0x3C;
inline constexpr std::size_t CrcStatus = 0x40;
inline constexpr std::size_t PhyConfig = 0x50;
inline constexpr std::size_t Length = 0x54;
inline constexpr std::size_t RxCrcInit = 0x58;
inline constexpr std::size_t RxCrcPoly = 0x5C;
inline constexpr std::size_t RxAccessAddress = 0x60;
inline constexpr std::size_t RxAccessAddressEx = 0x64;
} // namespace bb

namespace rfend {
inline constexpr std::size_t Control = 0x04;
inline constexpr std::size_t Control2 = 0x08;
inline constexpr std::size_t Control3 = 0x0C;
inline constexpr std::size_t PhyConfig = 0x34;
inline constexpr std::size_t TxBias = 0x28;
inline constexpr std::size_t TxControl = 0x2C;
inline constexpr std::size_t TxControl2 = 0x30;
inline constexpr std::size_t DacRef = 0x38;
inline constexpr std::size_t Power = 0x3C;
inline constexpr std::size_t ChannelDivider = 0x44;
inline constexpr std::size_t TuneControl = 0x48;
inline constexpr std::size_t FilterControl = 0x50;
inline constexpr std::size_t AdcControl = 0x58;
inline constexpr std::size_t RxControl = 0x5C;
inline constexpr std::size_t TuneResult = 0x90;
inline constexpr std::size_t GainResult = 0x94;
inline constexpr std::size_t FilterResult = 0x9C;
inline constexpr std::size_t TxTable0 = 0xA0;
} // namespace rfend

} // namespace radio_reg

} // namespace ch572
