#pragma once

#include <cstddef>
#include <cstdint>

// CH572/CH570 register map.
//
// SVD blocks covered here: PFIC, Systick, SYS, System_control, RTC, CMP,
// KEYSCAN, GPIO, TMR, ENCODER, UART, SPI, I2C, PWMX and USBFS. The FlashROM
// controller is vendor-derived because the SVD lists only its address, and the
// BLE block is an address-only placeholder in the SVD.
//
// Addresses and bit fields are cross-checked against:
//   - CH572.svd (via the ch32-rs/ch5xx-data SVD-derived database)
//   - WCH CH572SFR.h V1.04 and RVMSIS/core_riscv.h from the CH572 SDK
//   - the vendor libCH57xRF.a for the undocumented RFEND block
//
// Field access types (RF/RO/RZ/WO/WA/WZ/RW/RWA/RW1) matter on this part:
// RWA registers are write-protected and need a sysreg::SafeAccess guard.

namespace ch572
{

namespace mmio
{

template <typename T>
inline volatile T& ref(std::uintptr_t address) noexcept
{
    return *reinterpret_cast<volatile T*>(address);
}

} // namespace mmio

inline void fence_i() noexcept
{
    asm volatile("fence.i" ::: "memory");
}

inline void nop() noexcept
{
    asm volatile("nop");
}

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

// WCH implements the global interrupt enable in the custom CSR 0x800
// (PFIC_EnableAllIRQ writes 0x800 = 0x88); mstatus alone does not gate PFIC
// interrupts. Must be set once before any peripheral interrupt is used.
inline void enable_global_irq() noexcept
{
    constexpr std::uint32_t kGlobalIrqEnable = 0x88u;
    asm volatile("csrw 0x800, %0" : : "r"(kGlobalIrqEnable) : "memory");
}

// Clears the global interrupt-enable bit while keeping the previous-enable
// shadow, matching the value the vendor PFIC_DisableAllIRQ() writes.
inline void disable_global_irq() noexcept
{
    constexpr std::uint32_t kGlobalIrqDisable = 0x80u;
    asm volatile("csrw 0x800, %0" : : "r"(kGlobalIrqDisable) : "memory");
}

class CriticalSection
{
  public:
    CriticalSection() noexcept
        : m_state(irq_save())
    {
    }

    ~CriticalSection() noexcept
    {
        irq_restore(m_state);
    }

    CriticalSection(const CriticalSection&)            = delete;
    CriticalSection& operator=(const CriticalSection&) = delete;

  private:
    std::uint32_t m_state;
};

// ---------------------------------------------------------------------------
// Interrupt controller and core timer
// ---------------------------------------------------------------------------

enum class Irq : std::uint32_t
{
    Reset           = 0,
    Nmi             = 2,
    Exception       = 3,
    EcallMachine    = 5,
    EcallUser       = 8,
    Breakpoint      = 9,
    SystemTick      = 12,
    Software        = 14,
    GpioA           = 17,
    Spi             = 19,
    BleBb           = 20,
    BleLle          = 21,
    Usb             = 22,
    Timer           = 24,
    Uart            = 27,
    Rtc             = 28,
    Comparator      = 29,
    I2c             = 30,
    Pwm             = 31,
    KeyScan         = 33,
    Encoder         = 34,
    WatchdogBattery = 35,
};

struct Pfic
{
    volatile std::uint32_t isr[8];
    volatile std::uint32_t ipr[8];
    volatile std::uint32_t ithresdr;
    std::uint32_t          reserved0;
    volatile std::uint32_t cfgr; // 0x48: RESETSYS/KEYCODE
    volatile std::uint32_t gisr;
    volatile std::uint8_t  vtfidr[4];
    std::uint8_t           reserved1[0x0C];
    volatile std::uint32_t vtfaddr[4];
    std::uint8_t           reserved2[0x90];
    volatile std::uint32_t ienr[8];
    std::uint8_t           reserved3[0x60];
    volatile std::uint32_t irer[8];
    std::uint8_t           reserved4[0x60];
    volatile std::uint32_t ipsr[8];
    std::uint8_t           reserved5[0x60];
    volatile std::uint32_t iprr[8];
    std::uint8_t           reserved6[0x60];
    volatile std::uint32_t iactr[8];
    std::uint8_t           reserved7[0xE0];
    // Vendor core_riscv.h uses one byte per IRQ at 0x400; the SVD describes
    // 32-bit priority registers with a 0x20 stride. The vendor layout is the
    // one PFIC_SetPriority() writes, so keep it.
    volatile std::uint8_t  iprior[256];
    std::uint8_t           reserved8[0x810];
    volatile std::uint32_t sctlr;
};

static_assert(offsetof(Pfic, cfgr) == 0x48, "PFIC CFGR offset");
static_assert(offsetof(Pfic, gisr) == 0x4C, "PFIC GISR offset");
static_assert(offsetof(Pfic, ienr) == 0x100, "PFIC IENR offset");
static_assert(offsetof(Pfic, iprior) == 0x400, "PFIC IPRIOR offset");
static_assert(offsetof(Pfic, sctlr) == 0xD10, "PFIC SCTLR offset");

inline Pfic& pfic() noexcept
{
    return *reinterpret_cast<Pfic*>(0xE000E000u);
}

// PFIC CFGR is write-only; RESETSYS needs KEYCODE in bits 31:16.
inline constexpr std::uint32_t kPficResetSys     = 1u << 7;
inline constexpr std::uint32_t kPficKeyCodeShift = 16;

// PFIC SCTLR.
inline constexpr std::uint32_t kPficSleepOnExit  = 1u << 1;
inline constexpr std::uint32_t kPficSleepDeep    = 1u << 2;
inline constexpr std::uint32_t kPficWfiToWfe     = 1u << 3;
inline constexpr std::uint32_t kPficSevOnPending = 1u << 4;
inline constexpr std::uint32_t kPficSetEvent     = 1u << 5;

inline void                    enable_irq(Irq irq) noexcept
{
    const auto index   = static_cast<std::uint32_t>(irq) >> 5;
    const auto bit     = 1u << (static_cast<std::uint32_t>(irq) & 0x1Fu);
    pfic().ienr[index] = bit;
}

inline void disable_irq(Irq irq) noexcept
{
    const auto index   = static_cast<std::uint32_t>(irq) >> 5;
    const auto bit     = 1u << (static_cast<std::uint32_t>(irq) & 0x1Fu);
    pfic().irer[index] = bit;
    fence_i();
}

inline bool irq_pending(Irq irq) noexcept
{
    const auto index = static_cast<std::uint32_t>(irq) >> 5;
    const auto bit   = 1u << (static_cast<std::uint32_t>(irq) & 0x1Fu);
    return (pfic().ipr[index] & bit) != 0;
}

inline void clear_pending_irq(Irq irq) noexcept
{
    const auto index   = static_cast<std::uint32_t>(irq) >> 5;
    const auto bit     = 1u << (static_cast<std::uint32_t>(irq) & 0x1Fu);
    pfic().iprr[index] = bit;
}

// The CH572 implements two priority bits. Only "enabled" and "disabled" are
// distinguished by the vendor startup code.
inline void set_irq_priority(Irq irq, std::uint8_t priority) noexcept
{
    pfic().iprior[static_cast<std::uint32_t>(irq)] = priority != 0 ? 0x80u : 0x00u;
}

// WCH core SysTick: a 32-bit HCLK or HCLK/8 up/down counter at 0xE000F000.
// Writing ctrl() = kCtrlEnable starts it as a free-running counter without
// interrupts; now() is the counter value (CTLR=0x00, SR=0x04, CNT=0x08).
namespace systick
{

inline constexpr std::uintptr_t kBase = 0xE000F000u;

inline volatile std::uint32_t&  ctrl() noexcept
{
    return mmio::ref<std::uint32_t>(kBase + 0x00u);
}

inline constexpr std::uint32_t kCtrlEnable    = 1u << 0;  // STE
inline constexpr std::uint32_t kCtrlIntEnable = 1u << 1;  // STIE
inline constexpr std::uint32_t kCtrlClockHclk = 1u << 2;  // STCLK: 1=HCLK, 0=HCLK/8
inline constexpr std::uint32_t kCtrlReload    = 1u << 3;  // STRE
inline constexpr std::uint32_t kCtrlMode      = 1u << 4;  // MODE
inline constexpr std::uint32_t kCtrlSoftware  = 1u << 31; // SWIE

inline volatile std::uint32_t& status() noexcept
{
    return mmio::ref<std::uint32_t>(kBase + 0x04u);
}

inline constexpr std::uint32_t kStatusCountIf  = 1u << 0;
inline constexpr std::uint32_t kStatusSoftware = 1u << 31;

inline volatile std::uint32_t& now() noexcept
{
    return mmio::ref<std::uint32_t>(kBase + 0x08u);
}

inline volatile std::uint32_t& compare() noexcept
{
    return mmio::ref<std::uint32_t>(kBase + 0x10u);
}

inline volatile std::uint32_t& compare_high() noexcept
{
    return mmio::ref<std::uint32_t>(kBase + 0x14u);
}

inline bool expired(std::uint32_t deadline) noexcept
{
    return static_cast<std::int32_t>(now() - deadline) >= 0;
}

inline void delay(std::uint32_t ticks) noexcept
{
    const auto deadline = now() + ticks;
    while (static_cast<std::int32_t>(now() - deadline) < 0)
        ;
}

} // namespace systick

// ---------------------------------------------------------------------------
// SYS: watchdog, clock, sleep, pin alternate and power management
// ---------------------------------------------------------------------------

namespace sysreg
{

// Independent watchdog (0x40001000).
inline volatile std::uint32_t& iwdg_key() noexcept
{
    return mmio::ref<std::uint32_t>(0x40001000u);
}

inline volatile std::uint32_t& iwdg_cfg() noexcept
{
    return mmio::ref<std::uint32_t>(0x40001004u);
}

inline constexpr std::uint32_t kIwdgReloadMask     = 0x00000FFFu;
inline constexpr std::uint32_t kIwdgPrescalerShift = 12;
inline constexpr std::uint32_t kIwdgPrescalerMask  = 0x7u;
inline constexpr std::uint32_t kIwdgStopEnable     = 1u << 29;
inline constexpr std::uint32_t kIwdgWriteProtect   = 1u << 30;
inline constexpr std::uint32_t kIwdgEnable         = 1u << 31;

// Clock configuration (0x40001008).
inline volatile std::uint8_t&  clock_sys_cfg() noexcept
{
    return mmio::ref<std::uint8_t>(0x40001008u);
}

inline constexpr std::uint8_t kClockDividerMask = 0x1Fu;
inline constexpr std::uint8_t kClockSourceMask  = 0xC0u;
inline constexpr std::uint8_t kClockSourceXt32m = 0x00u;
inline constexpr std::uint8_t kClockSourcePll   = 0x40u;
inline constexpr std::uint8_t kClockSourceLsi   = 0xC0u;

// High-frequency clock power (0x4000100A).
inline volatile std::uint8_t& hfck_power_ctrl() noexcept
{
    return mmio::ref<std::uint8_t>(0x4000100Au);
}

inline constexpr std::uint8_t kClockPllOn     = 0x10;
inline constexpr std::uint8_t kClockXt32mKeep = 0x08;
inline constexpr std::uint8_t kClockXt32mOn   = 0x04;

// Sleep clock gating (0x4000100C..0x4000100F).
inline volatile std::uint8_t& sleep_clk_off0() noexcept
{
    return mmio::ref<std::uint8_t>(0x4000100Cu);
}

inline volatile std::uint8_t& sleep_clk_off1() noexcept
{
    return mmio::ref<std::uint8_t>(0x4000100Du);
}

inline volatile std::uint8_t& sleep_wake_ctrl() noexcept
{
    return mmio::ref<std::uint8_t>(0x4000100Eu);
}

inline volatile std::uint8_t& sleep_power_ctrl() noexcept
{
    return mmio::ref<std::uint8_t>(0x4000100Fu);
}

inline constexpr std::uint8_t  kSleepClkKeyscanWake = 0x80;
inline constexpr std::uint8_t  kSleepClkUart        = 0x10;
inline constexpr std::uint8_t  kSleepClkComparator  = 0x02;
inline constexpr std::uint8_t  kSleepClkTimer       = 0x01;

inline constexpr std::uint8_t  kSleepClkBle         = 0x80;
inline constexpr std::uint8_t  kSleepClkUsb         = 0x10;
inline constexpr std::uint8_t  kSleepClkI2c         = 0x08;
inline constexpr std::uint8_t  kSleepClkPwm         = 0x04;
inline constexpr std::uint8_t  kSleepClkAesCcm      = 0x02;
inline constexpr std::uint8_t  kSleepClkSpi         = 0x01;

inline constexpr std::uint8_t  kSleepWakeGpioLevel  = 0x80;
inline constexpr std::uint8_t  kSleepWakeEventMode  = 0x40;
inline constexpr std::uint8_t  kSleepWakeBattery    = 0x20;
inline constexpr std::uint8_t  kSleepWakeGpio       = 0x10;
inline constexpr std::uint8_t  kSleepWakeRtc        = 0x08;
inline constexpr std::uint8_t  kSleepWakeGpioEdge   = 0x04;
inline constexpr std::uint8_t  kSleepWakeEncoder    = 0x02;
inline constexpr std::uint8_t  kSleepWakeUsb        = 0x01;

inline constexpr std::uint8_t  kSleepRamRetMask     = 0xC0;
inline constexpr std::uint8_t  kSleepRamClockOff    = 0x10;
inline constexpr std::uint8_t  kSleepWakeDelayMask  = 0x07;

// I/O pin function selection (0x40001018..0x4000101E).
inline volatile std::uint16_t& pin_alternate() noexcept
{
    return mmio::ref<std::uint16_t>(0x40001018u);
}

inline volatile std::uint16_t& pin_alternate_high() noexcept
{
    return mmio::ref<std::uint16_t>(0x4000101Au);
}

inline constexpr std::uint16_t kPinDebugEnable  = 0x4000;
inline constexpr std::uint16_t kPinUsbEnable    = 0x2000;
inline constexpr std::uint16_t kUdpPullUpEnable = 0x1000;
inline constexpr std::uint16_t kPinAInputMask   = 0x0FFF;

inline constexpr std::uint16_t kPinUartRxdMask  = 0x0007;
inline constexpr std::uint16_t kPinUartTxdMask  = 0x0038;
inline constexpr std::uint16_t kPinTimerMask    = 0x00C0;
inline constexpr std::uint16_t kPinSpiCs        = 0x0100;
inline constexpr std::uint16_t kPinI2cMask      = 0x0600;
inline constexpr std::uint16_t kPinSpiClock     = 0x0800;
inline constexpr std::uint16_t kPinClock25m     = 0x1000;

inline volatile std::uint8_t&  long_reset_cfg() noexcept
{
    return mmio::ref<std::uint8_t>(0x4000101Cu);
}

inline volatile std::uint8_t& sleep_clk_off2() noexcept
{
    return mmio::ref<std::uint8_t>(0x4000101Du);
}

inline volatile std::uint16_t& sleep_wake_cfg() noexcept
{
    return mmio::ref<std::uint16_t>(0x4000101Eu);
}

inline constexpr std::uint8_t  kLongResetEnable     = 0x01;
inline constexpr std::uint8_t  kLongResetTimeMask   = 0x06;

inline constexpr std::uint8_t  kClockOffHclk        = 0x10;
inline constexpr std::uint8_t  kClockOffDebug       = 0x02;
inline constexpr std::uint8_t  kClockOffXrom        = 0x01;

inline constexpr std::uint16_t kSleepWakeOsclkReady = 0x0001;
inline constexpr std::uint16_t kSleepWakePreclkEn   = 0x0010;
inline constexpr std::uint16_t kSleepWakePreclkMask = 0x0060;
inline constexpr std::uint16_t kSleepWakeSafeAuto   = 0x0100;

// Power plan before WFI (0x40001020).
inline volatile std::uint16_t& power_plan() noexcept
{
    return mmio::ref<std::uint16_t>(0x40001020u);
}

inline constexpr std::uint16_t kPowerXrom        = 0x0001;
inline constexpr std::uint16_t kPowerRam12k      = 0x0002;
inline constexpr std::uint16_t kPowerCore        = 0x0004;
inline constexpr std::uint16_t kPowerExtend      = 0x0008;
inline constexpr std::uint16_t kPowerMainActive  = 0x0040;
inline constexpr std::uint16_t kPowerSysEnable   = 0x0080;
inline constexpr std::uint16_t kPowerLdo5vEnable = 0x0100;
inline constexpr std::uint16_t kPowerPlanEnable  = 0x8000;

inline volatile std::uint16_t& aux_power_adj() noexcept
{
    return mmio::ref<std::uint16_t>(0x40001022u);
}

inline constexpr std::uint16_t kAuxLdoAdjMask = 0x0007;
inline constexpr std::uint16_t kAuxIvrefMask  = 0x0F00;

// Battery detector (0x40001024..0x40001026).
inline volatile std::uint8_t&  bat_det_ctrl() noexcept
{
    return mmio::ref<std::uint8_t>(0x40001024u);
}

inline volatile std::uint8_t& bat_det_cfg() noexcept
{
    return mmio::ref<std::uint8_t>(0x40001025u);
}

inline volatile std::uint8_t& bat_status() noexcept
{
    return mmio::ref<std::uint8_t>(0x40001026u);
}

inline constexpr std::uint8_t  kBatteryLdoEnable     = 0x01;
inline constexpr std::uint8_t  kBatteryMonitorEnable = 0x02;
inline constexpr std::uint8_t  kBatteryLowIrqEnable  = 0x08;
inline constexpr std::uint8_t  kBatteryThresholdMask = 0x03;
inline constexpr std::uint8_t  kBatteryLowStatus     = 0x02;

// LSI oscillator (0x4000102C/0x4000102F).
inline volatile std::uint16_t& int_lsi_tune() noexcept
{
    return mmio::ref<std::uint16_t>(0x4000102Cu);
}

inline volatile std::uint8_t& lsi_config() noexcept
{
    return mmio::ref<std::uint8_t>(0x4000102Fu);
}

inline constexpr std::uint16_t kLsiTuneMask = 0x1FFF;
inline constexpr std::uint8_t  kLsiPowerOn  = 0x02;
inline constexpr std::uint8_t  kLsiClockPin = 0x80;

// -------------------------------------------------------------------------
// System_control: safe access, chip ID, reset, PLL and oscillator calibration
// -------------------------------------------------------------------------

inline volatile std::uint8_t&  safe_access_sig() noexcept
{
    return mmio::ref<std::uint8_t>(0x40001040u);
}

inline volatile std::uint8_t& chip_id() noexcept
{
    return mmio::ref<std::uint8_t>(0x40001041u);
}

inline volatile std::uint8_t& safe_access_id() noexcept
{
    return mmio::ref<std::uint8_t>(0x40001042u);
}

inline volatile std::uint8_t& watchdog_count() noexcept
{
    return mmio::ref<std::uint8_t>(0x40001043u);
}

inline volatile std::uint8_t& reset_status() noexcept
{
    return mmio::ref<std::uint8_t>(0x40001044u);
}

inline volatile std::uint8_t& global_rom_cfg() noexcept
{
    return mmio::ref<std::uint8_t>(0x40001044u);
}

inline volatile std::uint8_t& global_cfg_info() noexcept
{
    return mmio::ref<std::uint8_t>(0x40001045u);
}

inline volatile std::uint8_t& reset_watchdog_ctrl() noexcept
{
    return mmio::ref<std::uint8_t>(0x40001046u);
}

inline volatile std::uint8_t& glob_reset_keep() noexcept
{
    return mmio::ref<std::uint8_t>(0x40001047u);
}

inline volatile std::uint32_t& misc_ctrl() noexcept
{
    return mmio::ref<std::uint32_t>(0x40001048u);
}

// RF analog block enable field in misc_ctrl (bits 29:24).
inline constexpr std::uint32_t kRfAnalogMask   = 0x3Fu << 24;
inline constexpr std::uint32_t kRfAnalogEnable = 0x0Eu << 24;

inline volatile std::uint8_t&  pll_config() noexcept
{
    return mmio::ref<std::uint8_t>(0x4000104Bu);
}

inline volatile std::uint8_t& xt32m_tune() noexcept
{
    return mmio::ref<std::uint8_t>(0x4000104Eu);
}

inline volatile std::uint16_t& osc_cal_cnt() noexcept
{
    return mmio::ref<std::uint16_t>(0x40001050u);
}

inline volatile std::uint8_t& osc_cal_ov_cnt() noexcept
{
    return mmio::ref<std::uint8_t>(0x40001052u);
}

inline volatile std::uint8_t& osc_cal_ctrl() noexcept
{
    return mmio::ref<std::uint8_t>(0x40001053u);
}

inline volatile std::uint32_t& safe_access_sig2() noexcept
{
    return mmio::ref<std::uint32_t>(0x40001058u);
}

inline constexpr std::uint8_t  kSafeAccessSig1          = 0x57;
inline constexpr std::uint8_t  kSafeAccessSig2          = 0xA8;
inline constexpr std::uint8_t  kSafeAccessSig0          = 0x00;

inline constexpr std::uint8_t  kSafeAccessModeMask      = 0x03;
inline constexpr std::uint8_t  kSafeAccessActive        = 0x04;
inline constexpr std::uint8_t  kSafeAccessTimerMask     = 0xF8;

inline constexpr std::uint8_t  kSafeAccessIdValue       = 0x0C;
inline constexpr std::uint8_t  kChipIdCh572             = 0x72;
inline constexpr std::uint8_t  kChipIdCh570             = 0x70;

inline constexpr std::uint8_t  kResetFlagMask           = 0x07;
inline constexpr std::uint8_t  kResetFlagSoftware       = 0x00;
inline constexpr std::uint8_t  kResetFlagPowerOn        = 0x01;
inline constexpr std::uint8_t  kResetFlagWatchdog       = 0x02;
inline constexpr std::uint8_t  kResetFlagManual         = 0x03;
inline constexpr std::uint8_t  kResetFlagWakeShutdown   = 0x05;

inline constexpr std::uint8_t  kRomCodeOffset           = 0x10;
inline constexpr std::uint8_t  kRomControlEnable        = 0x20;
inline constexpr std::uint8_t  kRomCodeWriteMask        = 0xC0;

inline constexpr std::uint8_t  kCfgResetPinInput        = 0x04;
inline constexpr std::uint8_t  kCfgBootEnable           = 0x08;
inline constexpr std::uint8_t  kCfgResetPin             = 0x10;
inline constexpr std::uint8_t  kCfgBootLoader           = 0x20;

inline constexpr std::uint8_t  kSoftwareReset           = 0x01;
inline constexpr std::uint8_t  kBootLoadManual          = 0x01;
inline constexpr std::uint8_t  kWatchdogResetEnable     = 0x02;
inline constexpr std::uint8_t  kWatchdogIrqEnable       = 0x04;
inline constexpr std::uint8_t  kWatchdogIrqFlag         = 0x10;

inline constexpr std::uint8_t  kPllCfgDataMask          = 0x7F;

inline constexpr std::uint8_t  kXt32mBiasMask           = 0x03;
inline constexpr std::uint8_t  kXt32mLoadMask           = 0x70;

inline constexpr std::uint16_t kOscCalCountMask         = 0x3FFF;
inline constexpr std::uint16_t kOscCalOverflowClear     = 0x4000;
inline constexpr std::uint16_t kOscCalIrqFlag           = 0x8000;

inline constexpr std::uint8_t  kOscCalCountTotalMask    = 0x07;
inline constexpr std::uint8_t  kOscCalCountHalt         = 0x08;
inline constexpr std::uint8_t  kOscCalIrqEnable         = 0x10;
inline constexpr std::uint8_t  kOscCalCountEnable       = 0x20;
inline constexpr std::uint8_t  kOscCalCountEnd          = 0x40;
inline constexpr std::uint8_t  kOscCalCountClear        = 0x80;

inline constexpr std::uint32_t kSafeAccessSig2FuncMask  = 0x07000000;
inline constexpr std::uint32_t kSafeAccessSig2FlashHalt = 0x00800000;
inline constexpr std::uint32_t kSafeAccessSig2CfgLock   = 0x00400000;
inline constexpr std::uint32_t kSafeAccessSig2ReadProt  = 0x00200000;
inline constexpr std::uint32_t kSafeAccessSig2AutoOff   = 0x00100000;

// FlashROM controller (0x40001800). The SVD models the block only by address;
// these byte views match the vendor header.
inline volatile std::uint32_t& flash_data() noexcept
{
    return mmio::ref<std::uint32_t>(0x40001800u);
}

inline volatile std::uint32_t& flash_control() noexcept
{
    return mmio::ref<std::uint32_t>(0x40001804u);
}

inline volatile std::uint8_t& flash_sck() noexcept
{
    return mmio::ref<std::uint8_t>(0x40001805u);
}

inline volatile std::uint8_t& flash_ctrl() noexcept
{
    return mmio::ref<std::uint8_t>(0x40001806u);
}

inline volatile std::uint8_t& flash_cfg() noexcept
{
    return mmio::ref<std::uint8_t>(0x40001807u);
}

class SafeAccess
{
  public:
    SafeAccess() noexcept
        : m_state(irq_save())
    {
        fence_i();
        safe_access_sig() = kSafeAccessSig1;
        safe_access_sig() = kSafeAccessSig2;
        fence_i();
    }

    ~SafeAccess() noexcept
    {
        safe_access_sig() = kSafeAccessSig0;
        irq_restore(m_state);
        fence_i();
    }

    SafeAccess(const SafeAccess&)            = delete;
    SafeAccess& operator=(const SafeAccess&) = delete;

  private:
    std::uint32_t m_state;
};

// Clear the watchdog reset enables left set by the reset defaults.
inline void disable_watchdog_reset() noexcept
{
    SafeAccess access;
    reset_watchdog_ctrl() &= ~(kWatchdogResetEnable | kWatchdogIrqEnable);
}

} // namespace sysreg

// ---------------------------------------------------------------------------
// RTC (shares the 0x40001000 SYS window)
// ---------------------------------------------------------------------------

namespace rtc
{

inline volatile std::uint8_t& flag_ctrl() noexcept
{
    return mmio::ref<std::uint8_t>(0x40001030u);
}

inline volatile std::uint8_t& mode_ctrl() noexcept
{
    return mmio::ref<std::uint8_t>(0x40001031u);
}

inline volatile std::uint32_t& trigger() noexcept
{
    return mmio::ref<std::uint32_t>(0x40001034u);
}

inline volatile std::uint16_t& count_lsi() noexcept
{
    return mmio::ref<std::uint16_t>(0x40001038u);
}

inline volatile std::uint16_t& count_div1() noexcept
{
    return mmio::ref<std::uint16_t>(0x4000103Au);
}

inline volatile std::uint32_t& count_div2() noexcept
{
    return mmio::ref<std::uint32_t>(0x4000103Cu);
}

inline constexpr std::uint8_t  kTimerClear    = 0x10;
inline constexpr std::uint8_t  kTriggerClear  = 0x20;
inline constexpr std::uint8_t  kTimerFlag     = 0x40;
inline constexpr std::uint8_t  kTriggerFlag   = 0x80;

inline constexpr std::uint8_t  kTimerModeMask = 0x07;
inline constexpr std::uint8_t  kIgnoreBit0    = 0x08;
inline constexpr std::uint8_t  kTimerEnable   = 0x10;
inline constexpr std::uint8_t  kTriggerEnable = 0x20;
inline constexpr std::uint8_t  kLoadLow       = 0x40;
inline constexpr std::uint8_t  kLoadHigh      = 0x80;

inline constexpr std::uint32_t kCountDiv2Mask = 0x3FFF;

} // namespace rtc

// ---------------------------------------------------------------------------
// Comparator (0x40001054)
// ---------------------------------------------------------------------------

namespace cmp
{

inline volatile std::uint32_t& control() noexcept
{
    return mmio::ref<std::uint32_t>(0x40001054u);
}

inline volatile std::uint8_t& ctrl0() noexcept
{
    return mmio::ref<std::uint8_t>(0x40001054u);
}

inline volatile std::uint8_t& ctrl1() noexcept
{
    return mmio::ref<std::uint8_t>(0x40001055u);
}

inline volatile std::uint8_t& ctrl2() noexcept
{
    return mmio::ref<std::uint8_t>(0x40001056u);
}

inline volatile std::uint8_t& ctrl3() noexcept
{
    return mmio::ref<std::uint8_t>(0x40001057u);
}

inline constexpr std::uint8_t kNrefLevelMask    = 0xF0;
inline constexpr std::uint8_t kInputSelectMask  = 0x0C;
inline constexpr std::uint8_t kOutputToTimerCap = 0x02;
inline constexpr std::uint8_t kEnable           = 0x01;

inline constexpr std::uint8_t kOutputSelectMask = 0x0C;
inline constexpr std::uint8_t kOutputRising     = 0x0C;
inline constexpr std::uint8_t kOutputFalling    = 0x08;
inline constexpr std::uint8_t kOutputLow        = 0x04;
inline constexpr std::uint8_t kOutputHigh       = 0x00;
inline constexpr std::uint8_t kInterruptEnable  = 0x01;

inline constexpr std::uint8_t kInterruptFlag    = 0x01;
inline constexpr std::uint8_t kRealSignal       = 0x01;
inline constexpr std::uint8_t kAmpOutSignal     = 0x02;

} // namespace cmp

// ---------------------------------------------------------------------------
// KEYSCAN (0x40001064)
// ---------------------------------------------------------------------------

namespace keyscan
{

inline volatile std::uint16_t& ctrl() noexcept
{
    return mmio::ref<std::uint16_t>(0x40001064u);
}

inline volatile std::uint8_t& int_en() noexcept
{
    return mmio::ref<std::uint8_t>(0x40001066u);
}

inline volatile std::uint8_t& int_flag() noexcept
{
    return mmio::ref<std::uint8_t>(0x40001067u);
}

inline volatile std::uint32_t& number() noexcept
{
    return mmio::ref<std::uint32_t>(0x40001068u);
}

inline constexpr std::uint16_t kClearWakeupEnable   = 0x4000;
inline constexpr std::uint16_t kScanEndWakeEnable   = 0x2000;
inline constexpr std::uint16_t kPinScanMask         = 0x1F00;
inline constexpr std::uint16_t kScanClockDivMask    = 0x00F0;
inline constexpr std::uint16_t kScanCountEndMask    = 0x000E;
inline constexpr std::uint16_t kScanStartEnable     = 0x0001;

inline constexpr std::uint8_t  kScanEndIrqEnable    = 0x02;
inline constexpr std::uint8_t  kKeyPressedIrqEnable = 0x01;
inline constexpr std::uint8_t  kScanEndIrqFlag      = 0x02;
inline constexpr std::uint8_t  kKeyPressedIrqFlag   = 0x01;

inline constexpr std::uint32_t kScanCountMask       = 0x00700000;
inline constexpr std::uint32_t kScanNumberMask      = 0x000FFFFF;

} // namespace keyscan

// ---------------------------------------------------------------------------
// GPIO port A (0x40001090)
// ---------------------------------------------------------------------------

namespace gpio
{

namespace pa
{

inline constexpr std::uintptr_t kBase = 0x400010A0u;

inline volatile std::uint32_t&  dir() noexcept
{
    return mmio::ref<std::uint32_t>(kBase + 0x00u);
}

inline volatile std::uint32_t& pin() noexcept
{
    return mmio::ref<std::uint32_t>(kBase + 0x04u);
}

inline volatile std::uint32_t& out() noexcept
{
    return mmio::ref<std::uint32_t>(kBase + 0x08u);
}

inline volatile std::uint32_t& clear() noexcept
{
    return mmio::ref<std::uint32_t>(kBase + 0x0Cu);
}

inline volatile std::uint32_t& pull_up() noexcept
{
    return mmio::ref<std::uint32_t>(kBase + 0x10u);
}

inline volatile std::uint32_t& pull_down() noexcept
{
    return mmio::ref<std::uint32_t>(kBase + 0x14u);
}

inline volatile std::uint32_t& set() noexcept
{
    return mmio::ref<std::uint32_t>(kBase + 0x18u);
}

} // namespace pa

// CH572 exposes PA0..PA11 only.
inline constexpr std::uint32_t kPin0  = 1u << 0;
inline constexpr std::uint32_t kPin1  = 1u << 1;
inline constexpr std::uint32_t kPin2  = 1u << 2;
inline constexpr std::uint32_t kPin3  = 1u << 3;
inline constexpr std::uint32_t kPin4  = 1u << 4;
inline constexpr std::uint32_t kPin5  = 1u << 5;
inline constexpr std::uint32_t kPin6  = 1u << 6;
inline constexpr std::uint32_t kPin7  = 1u << 7;
inline constexpr std::uint32_t kPin8  = 1u << 8;
inline constexpr std::uint32_t kPin9  = 1u << 9;
inline constexpr std::uint32_t kPin10 = 1u << 10;
inline constexpr std::uint32_t kPin11 = 1u << 11;

// GPIO interrupt control block (0x40001090..0x4000109C). PA_INT_EDGE_TYPE is
// missing from the published SVD but present in the datasheet (table 7-8).
inline volatile std::uint16_t& int_en() noexcept
{
    return mmio::ref<std::uint16_t>(0x40001090u);
}

inline volatile std::uint16_t& int_mode() noexcept
{
    return mmio::ref<std::uint16_t>(0x40001094u);
}

inline volatile std::uint16_t& int_edge_type() noexcept
{
    return mmio::ref<std::uint16_t>(0x40001096u);
}

inline volatile std::uint16_t& int_flag() noexcept
{
    return mmio::ref<std::uint16_t>(0x4000109Cu);
}

inline constexpr std::uint16_t kIntModeLevel       = 0x0000u;
inline constexpr std::uint16_t kIntModeEdge        = 0xFFFFu;
inline constexpr std::uint16_t kIntEdgePolarity    = 0x0000u;
inline constexpr std::uint16_t kIntEdgeNonPolarity = 0xFFFFu;

} // namespace gpio

// ---------------------------------------------------------------------------
// TMR (0x40002400)
//
// The published SVD encodes TMR_CTRL_MOD at 0x400; the vendor header and the
// datasheet place the block at 0x00, which is what the hardware decodes.
// ---------------------------------------------------------------------------

namespace tmr
{

inline constexpr std::uintptr_t kBase = 0x40002400u;

inline volatile std::uint32_t&  control() noexcept
{
    return mmio::ref<std::uint32_t>(kBase + 0x00u);
}

inline volatile std::uint8_t& ctrl_mod() noexcept
{
    return mmio::ref<std::uint8_t>(kBase + 0x00u);
}

inline volatile std::uint8_t& ctrl_dma() noexcept
{
    return mmio::ref<std::uint8_t>(kBase + 0x01u);
}

inline volatile std::uint8_t& inter_en() noexcept
{
    return mmio::ref<std::uint8_t>(kBase + 0x02u);
}

inline volatile std::uint8_t& int_flag() noexcept
{
    return mmio::ref<std::uint8_t>(kBase + 0x06u);
}

inline volatile std::uint8_t& fifo_count() noexcept
{
    return mmio::ref<std::uint8_t>(kBase + 0x07u);
}

inline volatile std::uint32_t& count() noexcept
{
    return mmio::ref<std::uint32_t>(kBase + 0x08u);
}

inline volatile std::uint32_t& cnt_end() noexcept
{
    return mmio::ref<std::uint32_t>(kBase + 0x0Cu);
}

inline volatile std::uint32_t& fifo() noexcept
{
    return mmio::ref<std::uint32_t>(kBase + 0x10u);
}

inline volatile std::uint32_t& dma_now() noexcept
{
    return mmio::ref<std::uint32_t>(kBase + 0x14u);
}

inline volatile std::uint32_t& dma_beg() noexcept
{
    return mmio::ref<std::uint32_t>(kBase + 0x18u);
}

inline volatile std::uint32_t& dma_end() noexcept
{
    return mmio::ref<std::uint32_t>(kBase + 0x1Cu);
}

inline constexpr std::uint8_t  kPwmRepeatMask    = 0xC0;
inline constexpr std::uint8_t  kCaptureEdgeMask  = 0xC0;
inline constexpr std::uint8_t  kOutPolarity      = 0x10;
inline constexpr std::uint8_t  kCaptureCount     = 0x10;
inline constexpr std::uint8_t  kOutEnable        = 0x08;
inline constexpr std::uint8_t  kCountEnable      = 0x04;
inline constexpr std::uint8_t  kAllClear         = 0x02;
inline constexpr std::uint8_t  kModeInput        = 0x01;

inline constexpr std::uint8_t  kDmaLoop          = 0x04;
inline constexpr std::uint8_t  kDmaEnable        = 0x01;

inline constexpr std::uint8_t  kIrqFifoOverflow  = 0x10;
inline constexpr std::uint8_t  kIrqDmaEnd        = 0x08;
inline constexpr std::uint8_t  kIrqFifoHalf      = 0x04;
inline constexpr std::uint8_t  kIrqDataAction    = 0x02;
inline constexpr std::uint8_t  kIrqCycleEnd      = 0x01;

inline constexpr std::uint8_t  kFlagFifoOverflow = 0x10;
inline constexpr std::uint8_t  kFlagDmaEnd       = 0x08;
inline constexpr std::uint8_t  kFlagFifoHalf     = 0x04;
inline constexpr std::uint8_t  kFlagDataAction   = 0x02;
inline constexpr std::uint8_t  kFlagCycleEnd     = 0x01;

inline constexpr std::uint32_t kCountMask        = 0x03FFFFFFu;
inline constexpr std::uint32_t kCntEndMask       = 0x03FFFFFFu;

} // namespace tmr

// ---------------------------------------------------------------------------
// ENCODER (0x40002420)
//
// The SVD lists these registers as offsets 0x20..0x28 inside the TMR window;
// this header uses the absolute sub-block base instead.
// ---------------------------------------------------------------------------

namespace encoder
{

inline constexpr std::uintptr_t kBase = 0x40002420u;

inline volatile std::uint32_t&  control() noexcept
{
    return mmio::ref<std::uint32_t>(kBase + 0x00u);
}

inline volatile std::uint8_t& ctrl() noexcept
{
    return mmio::ref<std::uint8_t>(kBase + 0x00u);
}

inline volatile std::uint8_t& int_en() noexcept
{
    return mmio::ref<std::uint8_t>(kBase + 0x01u);
}

inline volatile std::uint8_t& int_flag() noexcept
{
    return mmio::ref<std::uint8_t>(kBase + 0x02u);
}

inline volatile std::uint32_t& cnt_end() noexcept
{
    return mmio::ref<std::uint32_t>(kBase + 0x04u);
}

inline volatile std::uint32_t& count() noexcept
{
    return mmio::ref<std::uint32_t>(kBase + 0x08u);
}

inline constexpr std::uint8_t kDirection       = 0x20;
inline constexpr std::uint8_t kReadClearEnable = 0x08;
inline constexpr std::uint8_t kSmsModeMask     = 0x06;
inline constexpr std::uint8_t kSmsT1Edge       = 0x04;
inline constexpr std::uint8_t kSmsT2Edge       = 0x02;
inline constexpr std::uint8_t kSmsT12Edge      = 0x06;
inline constexpr std::uint8_t kStartEnable     = 0x01;

inline constexpr std::uint8_t kIrqDecrement    = 0x02;
inline constexpr std::uint8_t kIrqIncrement    = 0x01;
inline constexpr std::uint8_t kFlagDecrement   = 0x02;
inline constexpr std::uint8_t kFlagIncrement   = 0x01;

} // namespace encoder

// ---------------------------------------------------------------------------
// UART0 (0x40003400)
// ---------------------------------------------------------------------------

namespace uart
{

inline constexpr std::uintptr_t kBase = 0x40003400u;

inline volatile std::uint8_t&   mcr() noexcept
{
    return mmio::ref<std::uint8_t>(kBase + 0x00u);
}

inline volatile std::uint8_t& ier() noexcept
{
    return mmio::ref<std::uint8_t>(kBase + 0x01u);
}

inline volatile std::uint8_t& fcr() noexcept
{
    return mmio::ref<std::uint8_t>(kBase + 0x02u);
}

inline volatile std::uint8_t& lcr() noexcept
{
    return mmio::ref<std::uint8_t>(kBase + 0x03u);
}

inline volatile std::uint8_t& iir() noexcept
{
    return mmio::ref<std::uint8_t>(kBase + 0x04u);
}

inline volatile std::uint8_t& lsr() noexcept
{
    return mmio::ref<std::uint8_t>(kBase + 0x05u);
}

inline volatile std::uint8_t& rbr() noexcept
{
    return mmio::ref<std::uint8_t>(kBase + 0x08u);
}

inline volatile std::uint8_t& thr() noexcept
{
    return mmio::ref<std::uint8_t>(kBase + 0x08u);
}

inline volatile std::uint8_t& rfc() noexcept
{
    return mmio::ref<std::uint8_t>(kBase + 0x0Au);
}

inline volatile std::uint8_t& tfc() noexcept
{
    return mmio::ref<std::uint8_t>(kBase + 0x0Bu);
}

inline volatile std::uint8_t& dll() noexcept
{
    return mmio::ref<std::uint8_t>(kBase + 0x0Cu);
}

inline volatile std::uint8_t& dlm() noexcept
{
    return mmio::ref<std::uint8_t>(kBase + 0x0Du);
}

inline volatile std::uint8_t& div() noexcept
{
    return mmio::ref<std::uint8_t>(kBase + 0x0Eu);
}

inline volatile std::uint8_t& adr() noexcept
{
    return mmio::ref<std::uint8_t>(kBase + 0x0Fu);
}

inline constexpr std::uint8_t kMcrOut2             = 0x08;
inline constexpr std::uint8_t kMcrIrqOutput        = 0x08;

inline constexpr std::uint8_t kIerTxdEnable        = 0x40;
inline constexpr std::uint8_t kIerLineStatus       = 0x04;
inline constexpr std::uint8_t kIerThrEmpty         = 0x02;
inline constexpr std::uint8_t kIerRecvReady        = 0x01;

inline constexpr std::uint8_t kFcrTriggerMask      = 0xC0;
inline constexpr std::uint8_t kFcrFifoEnable       = 0x01;

inline constexpr std::uint8_t kLcrDlab             = 0x80;
inline constexpr std::uint8_t kLcrBreakEnable      = 0x40;
inline constexpr std::uint8_t kLcrParityMask       = 0x30;
inline constexpr std::uint8_t kLcrParityEnable     = 0x08;
inline constexpr std::uint8_t kLcrStopBits         = 0x04;
inline constexpr std::uint8_t kLcrWordSizeMask     = 0x03;

inline constexpr std::uint8_t kIirFifoIdMask       = 0xC0;
inline constexpr std::uint8_t kIirIntMask          = 0x0F;
inline constexpr std::uint8_t kIirNoInterrupt      = 0x01;

inline constexpr std::uint8_t kLsrRxFifoError      = 0x80;
inline constexpr std::uint8_t kLsrTxAllEmpty       = 0x40;
inline constexpr std::uint8_t kLsrTxFifoEmpty      = 0x20;
inline constexpr std::uint8_t kLsrBreakError       = 0x10;
inline constexpr std::uint8_t kLsrFrameError       = 0x08;
inline constexpr std::uint8_t kLsrParityError      = 0x04;
inline constexpr std::uint8_t kLsrOverrun          = 0x02;
inline constexpr std::uint8_t kLsrDataReady        = 0x01;

inline constexpr std::uint8_t kIirRecvTimeout      = 0x0C;
inline constexpr std::uint8_t kIirLineStatus       = 0x06;
inline constexpr std::uint8_t kIirRecvReady        = 0x04;
inline constexpr std::uint8_t kIirThrEmpty         = 0x02;
inline constexpr std::uint8_t kIirNoInterruptValue = 0x01;

} // namespace uart

// ---------------------------------------------------------------------------
// SPI0 (0x40004000)
// ---------------------------------------------------------------------------

namespace spi
{

inline constexpr std::uintptr_t kBase = 0x40004000u;

inline volatile std::uint8_t&   ctrl_mod() noexcept
{
    return mmio::ref<std::uint8_t>(kBase + 0x00u);
}

inline volatile std::uint8_t& ctrl_cfg() noexcept
{
    return mmio::ref<std::uint8_t>(kBase + 0x01u);
}

inline volatile std::uint8_t& inter_en() noexcept
{
    return mmio::ref<std::uint8_t>(kBase + 0x02u);
}

inline volatile std::uint8_t& clock_div() noexcept
{
    return mmio::ref<std::uint8_t>(kBase + 0x03u);
}

inline volatile std::uint8_t& buffer() noexcept
{
    return mmio::ref<std::uint8_t>(kBase + 0x04u);
}

inline volatile std::uint8_t& run_flag() noexcept
{
    return mmio::ref<std::uint8_t>(kBase + 0x05u);
}

inline volatile std::uint8_t& int_flag() noexcept
{
    return mmio::ref<std::uint8_t>(kBase + 0x06u);
}

inline volatile std::uint8_t& fifo_count() noexcept
{
    return mmio::ref<std::uint8_t>(kBase + 0x07u);
}

inline volatile std::uint32_t& inter_cfg1() noexcept
{
    return mmio::ref<std::uint32_t>(kBase + 0x08u);
}

inline volatile std::uint8_t& int_type() noexcept
{
    return mmio::ref<std::uint8_t>(kBase + 0x08u);
}

inline volatile std::uint8_t& inter1_en() noexcept
{
    return mmio::ref<std::uint8_t>(kBase + 0x09u);
}

inline volatile std::uint8_t& int1_flag() noexcept
{
    return mmio::ref<std::uint8_t>(kBase + 0x0Au);
}

inline volatile std::uint16_t& total_cnt() noexcept
{
    return mmio::ref<std::uint16_t>(kBase + 0x0Cu);
}

inline volatile std::uint32_t& fifo() noexcept
{
    return mmio::ref<std::uint32_t>(kBase + 0x10u);
}

inline volatile std::uint8_t& fifo_count1() noexcept
{
    return mmio::ref<std::uint8_t>(kBase + 0x13u);
}

inline volatile std::uint32_t& dma_now() noexcept
{
    return mmio::ref<std::uint32_t>(kBase + 0x14u);
}

inline volatile std::uint32_t& dma_beg() noexcept
{
    return mmio::ref<std::uint32_t>(kBase + 0x18u);
}

inline volatile std::uint32_t& dma_end() noexcept
{
    return mmio::ref<std::uint32_t>(kBase + 0x1Cu);
}

inline constexpr std::uint8_t  kMisoOutputEnable   = 0x80;
inline constexpr std::uint8_t  kMosiOutputEnable   = 0x40;
inline constexpr std::uint8_t  kSckOutputEnable    = 0x20;
inline constexpr std::uint8_t  kFifoDirection      = 0x10;
inline constexpr std::uint8_t  kSlaveCommandMode   = 0x08;
inline constexpr std::uint8_t  kMasterClockMode3   = 0x08;
inline constexpr std::uint8_t  kTwoWireMode        = 0x04;
inline constexpr std::uint8_t  kAllClear           = 0x02;
inline constexpr std::uint8_t  kSlaveMode          = 0x01;

inline constexpr std::uint8_t  kMasterDelayEnable  = 0x40;
inline constexpr std::uint8_t  kLsbFirst           = 0x20;
inline constexpr std::uint8_t  kAutoClearIrq       = 0x10;
inline constexpr std::uint8_t  kDmaLoop            = 0x04;
inline constexpr std::uint8_t  kMasterClockInvert  = 0x02;
inline constexpr std::uint8_t  kDmaEnable          = 0x01;

inline constexpr std::uint8_t  kIrqFirstByte       = 0x80;
inline constexpr std::uint8_t  kIrqFifoOverflow    = 0x10;
inline constexpr std::uint8_t  kIrqDmaEnd          = 0x08;
inline constexpr std::uint8_t  kIrqFifoHalf        = 0x04;
inline constexpr std::uint8_t  kIrqByteEnd         = 0x02;
inline constexpr std::uint8_t  kIrqCountEnd        = 0x01;

inline constexpr std::uint8_t  kSlaveSelected      = 0x80;
inline constexpr std::uint8_t  kSlaveCsLoad        = 0x40;
inline constexpr std::uint8_t  kFifoReady          = 0x20;
inline constexpr std::uint8_t  kSlaveCommand       = 0x10;

inline constexpr std::uint8_t  kFlagFirstByte      = 0x80;
inline constexpr std::uint8_t  kFlagFree           = 0x40;
inline constexpr std::uint8_t  kFlagFifoOverflow   = 0x10;
inline constexpr std::uint8_t  kFlagDmaEnd         = 0x08;
inline constexpr std::uint8_t  kFlagFifoHalf       = 0x04;
inline constexpr std::uint8_t  kFlagByteEnd        = 0x02;
inline constexpr std::uint8_t  kFlagCountEnd       = 0x01;

inline constexpr std::uint32_t kInterCfgFifoFull   = 0x00020000u;
inline constexpr std::uint32_t kInterCfgFifoEmpty  = 0x00010000u;
inline constexpr std::uint32_t kInterCfgEnFull     = 0x00000200u;
inline constexpr std::uint32_t kInterCfgEnEmpty    = 0x00000100u;
inline constexpr std::uint32_t kInterCfgTypeMask   = 0x0000001Fu;

// Byte views of the same words (SVD: INT_TYPE at 0x08, INTER1_EN at 0x09,
// INT1_FLAG at 0x0A).
inline constexpr std::uint8_t  kIntTypeMask        = 0x1F;
inline constexpr std::uint8_t  kIrqFifoEmptyEnable = 0x01;
inline constexpr std::uint8_t  kIrqFifoFullEnable  = 0x02;
inline constexpr std::uint8_t  kFlagFifoEmpty      = 0x01;
inline constexpr std::uint8_t  kFlagFifoFull       = 0x02;

inline constexpr std::uint16_t kTotalCountMask     = 0x0FFF;

} // namespace spi

// ---------------------------------------------------------------------------
// I2C0 (0x40004800)
// ---------------------------------------------------------------------------

namespace i2c
{

inline constexpr std::uintptr_t kBase = 0x40004800u;

inline volatile std::uint16_t&  ctrl1() noexcept
{
    return mmio::ref<std::uint16_t>(kBase + 0x00u);
}

inline volatile std::uint16_t& ctrl2() noexcept
{
    return mmio::ref<std::uint16_t>(kBase + 0x04u);
}

inline volatile std::uint16_t& own_addr1() noexcept
{
    return mmio::ref<std::uint16_t>(kBase + 0x08u);
}

inline volatile std::uint16_t& own_addr2() noexcept
{
    return mmio::ref<std::uint16_t>(kBase + 0x0Cu);
}

inline volatile std::uint16_t& data() noexcept
{
    return mmio::ref<std::uint16_t>(kBase + 0x10u);
}

inline volatile std::uint16_t& status1() noexcept
{
    return mmio::ref<std::uint16_t>(kBase + 0x14u);
}

inline volatile std::uint16_t& status2() noexcept
{
    return mmio::ref<std::uint16_t>(kBase + 0x18u);
}

inline volatile std::uint16_t& clock_cfg() noexcept
{
    return mmio::ref<std::uint16_t>(kBase + 0x1Cu);
}

inline volatile std::uint16_t& rise_time() noexcept
{
    return mmio::ref<std::uint16_t>(kBase + 0x20u);
}

inline constexpr std::uint16_t kSoftwareReset    = 0x8000;
inline constexpr std::uint16_t kSmbusAlert       = 0x2000;
inline constexpr std::uint16_t kPacketError      = 0x1000;
inline constexpr std::uint16_t kAckPosition      = 0x0800;
inline constexpr std::uint16_t kAckEnable        = 0x0400;
inline constexpr std::uint16_t kStop             = 0x0200;
inline constexpr std::uint16_t kStart            = 0x0100;
inline constexpr std::uint16_t kNoStretch        = 0x0080;
inline constexpr std::uint16_t kGeneralCall      = 0x0040;
inline constexpr std::uint16_t kEnablePacket     = 0x0020;
inline constexpr std::uint16_t kArpEnable        = 0x0010;
inline constexpr std::uint16_t kSmbusHostType    = 0x0008;
inline constexpr std::uint16_t kSmbusMode        = 0x0002;
inline constexpr std::uint16_t kEnable           = 0x0001;

inline constexpr std::uint16_t kBufferIrqEnable  = 0x0400;
inline constexpr std::uint16_t kEventIrqEnable   = 0x0200;
inline constexpr std::uint16_t kErrorIrqEnable   = 0x0100;
inline constexpr std::uint16_t kFrequencyMask    = 0x003F;

inline constexpr std::uint16_t kAddressMode10    = 0x8000;
inline constexpr std::uint16_t kAddress9_8Mask   = 0x0300;
inline constexpr std::uint16_t kAddress7_1Mask   = 0x00FE;
inline constexpr std::uint16_t kAddress0         = 0x0001;
inline constexpr std::uint16_t kDualAddressMask  = 0x00FE;
inline constexpr std::uint16_t kDualEnable       = 0x0001;

inline constexpr std::uint16_t kSmbAlertFlag     = 0x8000;
inline constexpr std::uint16_t kTimeoutFlag      = 0x4000;
inline constexpr std::uint16_t kPacketErrorFlag  = 0x1000;
inline constexpr std::uint16_t kOverrunFlag      = 0x0800;
inline constexpr std::uint16_t kAckFailureFlag   = 0x0400;
inline constexpr std::uint16_t kArbitrationLost  = 0x0200;
inline constexpr std::uint16_t kBusErrorFlag     = 0x0100;
inline constexpr std::uint16_t kTxEmpty          = 0x0080;
inline constexpr std::uint16_t kRxNotEmpty       = 0x0040;
inline constexpr std::uint16_t kStopDetected     = 0x0010;
inline constexpr std::uint16_t kAddress10Sent    = 0x0008;
inline constexpr std::uint16_t kByteTransferDone = 0x0004;
inline constexpr std::uint16_t kAddressFlag      = 0x0002;
inline constexpr std::uint16_t kStartFlag        = 0x0001;

inline constexpr std::uint16_t kPacketErrorMask  = 0xFF00;
inline constexpr std::uint16_t kDualFlag         = 0x0080;
inline constexpr std::uint16_t kSmbusHost        = 0x0040;
inline constexpr std::uint16_t kSmbusDefault     = 0x0020;
inline constexpr std::uint16_t kGeneralCallFlag  = 0x0010;
inline constexpr std::uint16_t kTransmit         = 0x0004;
inline constexpr std::uint16_t kBusy             = 0x0002;
inline constexpr std::uint16_t kMasterMode       = 0x0001;

inline constexpr std::uint16_t kFastMode         = 0x8000;
inline constexpr std::uint16_t kDuty16_9         = 0x4000;
inline constexpr std::uint16_t kClockControlMask = 0x0FFF;
inline constexpr std::uint16_t kRiseTimeMask     = 0x003F;

} // namespace i2c

// ---------------------------------------------------------------------------
// PWMX (0x40005000)
// ---------------------------------------------------------------------------

namespace pwm
{

inline constexpr std::uintptr_t kBase = 0x40005000u;

inline volatile std::uint8_t&   out_en() noexcept
{
    return mmio::ref<std::uint8_t>(kBase + 0x00u);
}

inline volatile std::uint8_t& polarity() noexcept
{
    return mmio::ref<std::uint8_t>(kBase + 0x01u);
}

inline volatile std::uint8_t& config() noexcept
{
    return mmio::ref<std::uint8_t>(kBase + 0x02u);
}

inline volatile std::uint8_t& dma_ctrl() noexcept
{
    return mmio::ref<std::uint8_t>(kBase + 0x03u);
}

inline volatile std::uint16_t& pwm1_data16() noexcept
{
    return mmio::ref<std::uint16_t>(kBase + 0x04u);
}

inline volatile std::uint16_t& pwm2_data16() noexcept
{
    return mmio::ref<std::uint16_t>(kBase + 0x06u);
}

inline volatile std::uint16_t& pwm3_data16() noexcept
{
    return mmio::ref<std::uint16_t>(kBase + 0x08u);
}

inline volatile std::uint16_t& pwm4_data16() noexcept
{
    return mmio::ref<std::uint16_t>(kBase + 0x10u);
}

inline volatile std::uint16_t& pwm5_data16() noexcept
{
    return mmio::ref<std::uint16_t>(kBase + 0x12u);
}

inline volatile std::uint8_t& int_en() noexcept
{
    return mmio::ref<std::uint8_t>(kBase + 0x0Cu);
}

inline volatile std::uint8_t& int_flag() noexcept
{
    return mmio::ref<std::uint8_t>(kBase + 0x0Du);
}

inline volatile std::uint16_t& cycle_value() noexcept
{
    return mmio::ref<std::uint16_t>(kBase + 0x14u);
}

inline volatile std::uint16_t& cycle_value4_5() noexcept
{
    return mmio::ref<std::uint16_t>(kBase + 0x16u);
}

inline volatile std::uint16_t& clock_div() noexcept
{
    return mmio::ref<std::uint16_t>(kBase + 0x18u);
}

inline volatile std::uint32_t& dma_now() noexcept
{
    return mmio::ref<std::uint32_t>(kBase + 0x1Cu);
}

inline volatile std::uint32_t& dma_beg() noexcept
{
    return mmio::ref<std::uint32_t>(kBase + 0x20u);
}

inline volatile std::uint32_t& dma_end() noexcept
{
    return mmio::ref<std::uint32_t>(kBase + 0x24u);
}

inline constexpr std::uint8_t kPwm1OutEnable  = 0x01;
inline constexpr std::uint8_t kPwm2OutEnable  = 0x02;
inline constexpr std::uint8_t kPwm3OutEnable  = 0x04;
inline constexpr std::uint8_t kPwm4OutEnable  = 0x08;
inline constexpr std::uint8_t kPwm5OutEnable  = 0x10;

inline constexpr std::uint8_t kPwm1Polarity   = 0x01;
inline constexpr std::uint8_t kPwm2Polarity   = 0x02;
inline constexpr std::uint8_t kPwm3Polarity   = 0x04;
inline constexpr std::uint8_t kPwm4Polarity   = 0x08;
inline constexpr std::uint8_t kPwm5Polarity   = 0x10;

inline constexpr std::uint8_t kSyncEnable     = 0x80;
inline constexpr std::uint8_t kSyncStart      = 0x40;
inline constexpr std::uint8_t kChannel4_5     = 0x10;
inline constexpr std::uint8_t kStaggerEnable  = 0x08;
inline constexpr std::uint8_t kCycleModeMask  = 0x06;
inline constexpr std::uint8_t kCycleSelect    = 0x01;

inline constexpr std::uint8_t kDmaSelect      = 0x04;
inline constexpr std::uint8_t kDmaAddressLoop = 0x02;
inline constexpr std::uint8_t kDmaEnable      = 0x01;

inline constexpr std::uint8_t kIrqOverflow    = 0x40;
inline constexpr std::uint8_t kIrqDma         = 0x20;
inline constexpr std::uint8_t kIrqFifo        = 0x10;
inline constexpr std::uint8_t kIrqCycle4_5    = 0x04;
inline constexpr std::uint8_t kCycleIrqPoint  = 0x02;
inline constexpr std::uint8_t kIrqCycle       = 0x01;

inline constexpr std::uint8_t kFlagOverflow   = 0x10;
inline constexpr std::uint8_t kFlagDma        = 0x08;
inline constexpr std::uint8_t kFlagFifo       = 0x04;
inline constexpr std::uint8_t kFlagCycle4_5   = 0x02;
inline constexpr std::uint8_t kFlagCycle      = 0x01;

} // namespace pwm

// ---------------------------------------------------------------------------
// USBFS device controller (0x40008000)
// ---------------------------------------------------------------------------

namespace usbreg
{

inline constexpr std::uintptr_t kBase = 0x40008000u;

template <typename T>
inline volatile T& at(std::size_t offset) noexcept
{
    return mmio::ref<T>(kBase + offset);
}

inline volatile std::uint8_t& ctrl() noexcept
{
    return at<std::uint8_t>(0x00);
}

inline volatile std::uint8_t& dev_ctrl() noexcept
{
    return at<std::uint8_t>(0x01);
}

inline volatile std::uint8_t& host_ctrl() noexcept
{
    return at<std::uint8_t>(0x01);
}

inline volatile std::uint8_t& int_en() noexcept
{
    return at<std::uint8_t>(0x02);
}

inline volatile std::uint8_t& dev_addr() noexcept
{
    return at<std::uint8_t>(0x03);
}

inline volatile std::uint8_t& misc_status() noexcept
{
    return at<std::uint8_t>(0x05);
}

inline volatile std::uint8_t& int_fg() noexcept
{
    return at<std::uint8_t>(0x06);
}

inline volatile std::uint8_t& int_st() noexcept
{
    return at<std::uint8_t>(0x07);
}

inline volatile std::uint8_t& rx_len() noexcept
{
    return at<std::uint8_t>(0x08);
}

inline volatile std::uint8_t& ep4_1_mod() noexcept
{
    return at<std::uint8_t>(0x0C);
}

inline volatile std::uint8_t& ep2_3_mod() noexcept
{
    return at<std::uint8_t>(0x0D);
}

inline volatile std::uint8_t& ep5_6_7_mod() noexcept
{
    return at<std::uint8_t>(0x0E);
}

inline volatile std::uint32_t& ep_mode() noexcept
{
    return at<std::uint32_t>(0x70);
}

inline volatile std::uint16_t& ep0_dma() noexcept
{
    return at<std::uint16_t>(0x10);
}

inline volatile std::uint16_t& ep1_dma() noexcept
{
    return at<std::uint16_t>(0x14);
}

inline volatile std::uint16_t& ep2_dma() noexcept
{
    return at<std::uint16_t>(0x18);
}

inline volatile std::uint16_t& ep3_dma() noexcept
{
    return at<std::uint16_t>(0x1C);
}

inline volatile std::uint16_t& ep5_dma() noexcept
{
    return at<std::uint16_t>(0x54);
}

inline volatile std::uint16_t& ep6_dma() noexcept
{
    return at<std::uint16_t>(0x58);
}

inline volatile std::uint16_t& ep7_dma() noexcept
{
    return at<std::uint16_t>(0x5C);
}

inline volatile std::uint8_t& ep0_t_len() noexcept
{
    return at<std::uint8_t>(0x20);
}

inline volatile std::uint8_t& ep0_ctrl() noexcept
{
    return at<std::uint8_t>(0x22);
}

inline volatile std::uint8_t& ep1_t_len() noexcept
{
    return at<std::uint8_t>(0x24);
}

inline volatile std::uint8_t& ep1_ctrl() noexcept
{
    return at<std::uint8_t>(0x26);
}

inline volatile std::uint8_t& ep2_t_len() noexcept
{
    return at<std::uint8_t>(0x28);
}

inline volatile std::uint8_t& ep2_ctrl() noexcept
{
    return at<std::uint8_t>(0x2A);
}

inline volatile std::uint8_t& ep3_t_len() noexcept
{
    return at<std::uint8_t>(0x2C);
}

inline volatile std::uint8_t& ep3_ctrl() noexcept
{
    return at<std::uint8_t>(0x2E);
}

inline volatile std::uint8_t& ep4_t_len() noexcept
{
    return at<std::uint8_t>(0x30);
}

inline volatile std::uint8_t& ep4_ctrl() noexcept
{
    return at<std::uint8_t>(0x32);
}

inline volatile std::uint8_t& ep5_t_len() noexcept
{
    return at<std::uint8_t>(0x64);
}

inline volatile std::uint8_t& ep5_ctrl() noexcept
{
    return at<std::uint8_t>(0x66);
}

inline volatile std::uint8_t& ep6_t_len() noexcept
{
    return at<std::uint8_t>(0x68);
}

inline volatile std::uint8_t& ep6_ctrl() noexcept
{
    return at<std::uint8_t>(0x6A);
}

inline volatile std::uint8_t& ep7_t_len() noexcept
{
    return at<std::uint8_t>(0x6C);
}

inline volatile std::uint8_t& ep7_ctrl() noexcept
{
    return at<std::uint8_t>(0x6E);
}

// Host-mode aliases share the endpoint 1/2/3 registers.
inline volatile std::uint16_t& host_rx_dma() noexcept
{
    return ep2_dma();
}

inline volatile std::uint16_t& host_tx_dma() noexcept
{
    return ep3_dma();
}

inline volatile std::uint8_t& host_setup() noexcept
{
    return ep1_ctrl();
}

inline volatile std::uint8_t& host_ep_pid() noexcept
{
    return ep2_t_len();
}

inline volatile std::uint8_t& host_rx_ctrl() noexcept
{
    return ep2_ctrl();
}

inline volatile std::uint8_t& host_tx_len() noexcept
{
    return ep3_t_len();
}

inline volatile std::uint8_t& host_tx_ctrl() noexcept
{
    return ep3_ctrl();
}

inline volatile std::uint8_t& host_ep_mod() noexcept
{
    return ep2_3_mod();
}

// CTRL.
inline constexpr std::uint8_t  kUcHostMode         = 0x80;
inline constexpr std::uint8_t  kUcLowSpeed         = 0x40;
inline constexpr std::uint8_t  kUcDevPuEn          = 0x20;
inline constexpr std::uint8_t  kUcSysCtrlMask      = 0x30;
inline constexpr std::uint8_t  kUcIntBusy          = 0x08;
inline constexpr std::uint8_t  kUcResetSie         = 0x04;
inline constexpr std::uint8_t  kUcClrAll           = 0x02;
inline constexpr std::uint8_t  kUcDmaEn            = 0x01;

// UDEV_CTRL / UHOST_CTRL.
inline constexpr std::uint8_t  kUdPdDis            = 0x80;
inline constexpr std::uint8_t  kUdDpPin            = 0x20;
inline constexpr std::uint8_t  kUdDmPin            = 0x10;
inline constexpr std::uint8_t  kUdLowSpeed         = 0x04;
inline constexpr std::uint8_t  kUdGpBit            = 0x02;
inline constexpr std::uint8_t  kUdPortEn           = 0x01;
inline constexpr std::uint8_t  kUhBusReset         = 0x02;

// INT_EN.
inline constexpr std::uint8_t  kUieDevSof          = 0x80;
inline constexpr std::uint8_t  kUieDevNak          = 0x40;
inline constexpr std::uint8_t  kUieOneWire         = 0x20;
inline constexpr std::uint8_t  kUieFifoOverflow    = 0x10;
inline constexpr std::uint8_t  kUieHostSof         = 0x08;
inline constexpr std::uint8_t  kUieSuspend         = 0x04;
inline constexpr std::uint8_t  kUieTransfer        = 0x02;
inline constexpr std::uint8_t  kUieBusReset        = 0x01;
inline constexpr std::uint8_t  kUieDetect          = 0x01;

// INT_FG.
inline constexpr std::uint8_t  kUifNak             = 0x80;
inline constexpr std::uint8_t  kUifToggleOk        = 0x40;
inline constexpr std::uint8_t  kUifSieFree         = 0x20;
inline constexpr std::uint8_t  kUifFifoOverflow    = 0x10;
inline constexpr std::uint8_t  kUifHostSof         = 0x08;
inline constexpr std::uint8_t  kUifSuspend         = 0x04;
inline constexpr std::uint8_t  kUifTransfer        = 0x02;
inline constexpr std::uint8_t  kUifBusReset        = 0x01;
inline constexpr std::uint8_t  kUifDetect          = 0x01;

// INT_ST.
inline constexpr std::uint8_t  kUisSetupAct        = 0x80;
inline constexpr std::uint8_t  kUisToggleOk        = 0x40;
inline constexpr std::uint8_t  kUisToken1          = 0x20;
inline constexpr std::uint8_t  kUisToken0          = 0x10;
inline constexpr std::uint8_t  kMaskUisToken       = 0x30;
inline constexpr std::uint8_t  kMaskUisEndpoint    = 0x0F;
inline constexpr std::uint8_t  kUisTokenOut        = 0x00;
inline constexpr std::uint8_t  kUisTokenSof        = 0x10;
inline constexpr std::uint8_t  kUisTokenIn         = 0x20;
inline constexpr std::uint8_t  kUisTokenSetup      = 0x30;

// MIS_ST.
inline constexpr std::uint8_t  kUmsDeviceAttach    = 0x01;
inline constexpr std::uint8_t  kUmsDmLevel         = 0x02;
inline constexpr std::uint8_t  kUmsSuspend         = 0x04;
inline constexpr std::uint8_t  kUmsBusReset        = 0x08;
inline constexpr std::uint8_t  kUmsRxFifoReady     = 0x10;
inline constexpr std::uint8_t  kUmsSieFree         = 0x20;
inline constexpr std::uint8_t  kUmsSofActive       = 0x40;
inline constexpr std::uint8_t  kUmsSofPresent      = 0x80;

// UEP4_1_MOD.
inline constexpr std::uint8_t  kUep1RxEn           = 0x80;
inline constexpr std::uint8_t  kUep1TxEn           = 0x40;
inline constexpr std::uint8_t  kUep1BufMod         = 0x10;
inline constexpr std::uint8_t  kUep4RxEn           = 0x08;
inline constexpr std::uint8_t  kUep4TxEn           = 0x04;

// UEP2_3_MOD.
inline constexpr std::uint8_t  kUep3RxEn           = 0x80;
inline constexpr std::uint8_t  kUep3TxEn           = 0x40;
inline constexpr std::uint8_t  kUep3BufMod         = 0x10;
inline constexpr std::uint8_t  kUep2RxEn           = 0x08;
inline constexpr std::uint8_t  kUep2TxEn           = 0x04;
inline constexpr std::uint8_t  kUep2BufMod         = 0x01;

// UEP5_6_7_MOD.
inline constexpr std::uint8_t  kUep7RxEn           = 0x20;
inline constexpr std::uint8_t  kUep7TxEn           = 0x10;
inline constexpr std::uint8_t  kUep6RxEn           = 0x08;
inline constexpr std::uint8_t  kUep6TxEn           = 0x04;
inline constexpr std::uint8_t  kUep5RxEn           = 0x02;
inline constexpr std::uint8_t  kUep5TxEn           = 0x01;

// EP_MODE: endpoints 8..15, one bit per endpoint.
inline constexpr std::uint32_t kEp8To15TxEnable    = 0x000000FFu;
inline constexpr std::uint32_t kEp8To15RxEnable    = 0x0000FF00u;
inline constexpr std::uint32_t kEp8To14TxMultiplex = 0x00FE0000u;

// UEPn_CTRL (all endpoints share the layout).
inline constexpr std::uint8_t  kUepRTog            = 0x80;
inline constexpr std::uint8_t  kUepTTog            = 0x40;
inline constexpr std::uint8_t  kUepAutoTog         = 0x10;
inline constexpr std::uint8_t  kMaskUepRRes        = 0x0C;
inline constexpr std::uint8_t  kUepRResAck         = 0x00;
inline constexpr std::uint8_t  kUepRResTimeout     = 0x04;
inline constexpr std::uint8_t  kUepRResNak         = 0x08;
inline constexpr std::uint8_t  kUepRResStall       = 0x0C;
inline constexpr std::uint8_t  kMaskUepTRes        = 0x03;
inline constexpr std::uint8_t  kUepTResAck         = 0x00;
inline constexpr std::uint8_t  kUepTResTimeout     = 0x01;
inline constexpr std::uint8_t  kUepTResNak         = 0x02;
inline constexpr std::uint8_t  kUepTResStall       = 0x03;

} // namespace usbreg

// ---------------------------------------------------------------------------
// 2.4 GHz radio register windows
//
// The CH572 SVD contains BLE only as an address placeholder (0x4000C000), so
// these windows are vendor-derived. LLE/BB come from the vendor BLE library;
// RFEND is absent from the datasheet and SVD entirely.
// ---------------------------------------------------------------------------

namespace radio_reg
{

inline constexpr std::uintptr_t kLleBase   = 0x4000C200u;
inline constexpr std::uintptr_t kBbBase    = 0x4000C100u;
inline constexpr std::uintptr_t kRfEndBase = 0x4000D000u;

template <std::uintptr_t Base>
struct Window
{
    static volatile std::uint32_t& r32(std::size_t offset) noexcept
    {
        return mmio::ref<std::uint32_t>(Base + offset);
    }

    static volatile std::uint16_t& r16(std::size_t offset) noexcept
    {
        return mmio::ref<std::uint16_t>(Base + offset);
    }

    static volatile std::uint8_t& r8(std::size_t offset) noexcept
    {
        return mmio::ref<std::uint8_t>(Base + offset);
    }
};

using Lle   = Window<kLleBase>;
using Bb    = Window<kBbBase>;
using RfEnd = Window<kRfEndBase>;

namespace lle
{
inline constexpr std::size_t   Control          = 0x00;
inline constexpr std::size_t   Config           = 0x04;
inline constexpr std::size_t   WaitTime         = 0x06;
inline constexpr std::size_t   Irq              = 0x08;
inline constexpr std::size_t   Config2          = 0x0C;
inline constexpr std::size_t   PhyControl       = 0x50;
inline constexpr std::size_t   Config54         = 0x54;
inline constexpr std::size_t   Timer            = 0x64;
inline constexpr std::size_t   TxDma            = 0x78;
inline constexpr std::size_t   RxDma            = 0x7C;

// Raw interrupt bits. The hardware names come from the five LLE timers; the
// radio driver uses timer 0 as the RX timeout and timer 4 as the generic timer.
inline constexpr std::uint32_t kIrqRx           = 1u << 0;
inline constexpr std::uint32_t kIrqTx           = 1u << 1;
inline constexpr std::uint32_t kIrqTimer0       = 1u << 17;
inline constexpr std::uint32_t kIrqTimer1       = 1u << 18;
inline constexpr std::uint32_t kIrqTimer2       = 1u << 19;
inline constexpr std::uint32_t kIrqTimer3       = 1u << 20;
inline constexpr std::uint32_t kIrqTimer4       = 1u << 21;

// Control commands and initial configuration used by the vendor BLE library.
inline constexpr std::uint32_t kControlRx       = 1u;
inline constexpr std::uint32_t kControlTx       = 2u;
inline constexpr std::uint32_t kControlStop     = 8u;
inline constexpr std::uint32_t kControlModeMask = 3u;
inline constexpr std::uint32_t kConfigInit      = 0x1Eu;
inline constexpr std::uint32_t kConfig2Init     = 0x20003u;
} // namespace lle

namespace bb
{
inline constexpr std::size_t   Control                 = 0x00;
inline constexpr std::size_t   CrcInit                 = 0x04;
inline constexpr std::size_t   CrcPoly                 = 0x08;
inline constexpr std::size_t   AccessAddress           = 0x0C;
inline constexpr std::size_t   AccessAddressEx         = 0x10;
inline constexpr std::size_t   Control1                = 0x14;
inline constexpr std::size_t   Control2                = 0x1C;
inline constexpr std::size_t   Control2High            = 0x1E;
inline constexpr std::size_t   Control3                = 0x24;
inline constexpr std::size_t   TxPower                 = 0x34;
inline constexpr std::size_t   RxConfig2               = 0x38;
inline constexpr std::size_t   RxConfig                = 0x3C;
inline constexpr std::size_t   CrcStatus               = 0x40;
inline constexpr std::size_t   RxCrcInit               = 0x58;
inline constexpr std::size_t   RxCrcPoly               = 0x5C;
inline constexpr std::size_t   RxAccessAddress         = 0x60;
inline constexpr std::size_t   RxAccessAddressEx       = 0x64;

// Control register fields.
inline constexpr std::uint32_t kControlChannelMask     = 0x3Fu;
inline constexpr std::uint32_t kControlWhiteningOff    = 1u << 6;
inline constexpr std::uint32_t kControlPhyMask         = 0x3u << 8;
inline constexpr std::uint32_t kControlModeMask        = 0x3u << 16;
inline constexpr std::uint32_t kControlCalibMode       = 2u << 16;
inline constexpr std::uint32_t kControlCalibrationMode = 1u << 21;
inline constexpr std::uint32_t kControlIdleMode        = 1u << 16;
inline constexpr std::uint32_t kControlStartTx         = 1u << 28;
inline constexpr std::uint32_t kControl3PhyMask        = 0x3u << 25;

// TX power register fields.
inline constexpr std::uint32_t kTxPowerLevelMask       = 0x7Fu << 25;
inline constexpr std::uint32_t kTxPowerLevelShift      = 25;
inline constexpr std::uint32_t kTxPowerLowFieldMask    = 0x7Fu;
inline constexpr std::uint32_t kTxPowerLevelFlag       = 0x40u;
inline constexpr std::uint32_t kTxPowerModeMask        = 0x3u;
inline constexpr std::uint32_t kTxPowerModeCalibration = 1u;

// CRC status fields.
inline constexpr std::uint32_t kCrcStatusClear         = 0xFFFFu;
inline constexpr std::uint32_t kCrcStatusRxAck         = 0x60u;
inline constexpr std::uint32_t kCrcStatusDone          = 1u << 6;
inline constexpr std::uint32_t kCrcOkShift             = 5;
inline constexpr std::uint32_t kCrcOkMask              = 1u;
inline constexpr std::uint32_t kCrcPolyHighMask        = 0xFF000000u;
inline constexpr std::uint32_t kCrcPolyLowMask         = 0x00FFFFFFu;

// RX window registers.
inline constexpr std::uint32_t kRxControl1Mask         = 0x3Fu;
inline constexpr std::uint32_t kRxControl2Mask         = 0x3FFu;
} // namespace bb

namespace rfend
{
// Register offsets.
inline constexpr std::size_t   Control1                  = 0x04;
inline constexpr std::size_t   Control2                  = 0x08;
inline constexpr std::size_t   Control3                  = 0x0C;
inline constexpr std::size_t   TxBias                    = 0x28;
inline constexpr std::size_t   TxControl1                = 0x2C;
inline constexpr std::size_t   TxControl2                = 0x30;
inline constexpr std::size_t   PhyConfig                 = 0x34;
inline constexpr std::size_t   DacRef                    = 0x38;
inline constexpr std::size_t   Power                     = 0x3C;
inline constexpr std::size_t   TuneControl               = 0x48;
inline constexpr std::size_t   FilterControl             = 0x50;
inline constexpr std::size_t   AdcControl                = 0x58;
inline constexpr std::size_t   RxControl                 = 0x5C;
inline constexpr std::size_t   TuneResult                = 0x90;
inline constexpr std::size_t   TuneResultGa              = 0x94;
inline constexpr std::size_t   FilterResult              = 0x9C;
inline constexpr std::size_t   TuneTableCoLow            = 0xA0; // 5 words, 2401->2440 MHz
inline constexpr std::size_t   TuneTableCoHigh           = 0xB4; // 5 words, 2440->2480 MHz
inline constexpr std::size_t   TuneTableGa               = 0xC8; // 3 words, nGA correction

// Control1: one strobe per calibration step.
inline constexpr std::uint32_t kControl1TuneStart        = 1u << 0;
inline constexpr std::uint32_t kControl1TxCoarse         = 1u << 4;
inline constexpr std::uint32_t kControl1TxFine           = 1u << 8;
inline constexpr std::uint32_t kControl1RxFilter         = 1u << 12;
inline constexpr std::uint32_t kControl1RxAdc            = 1u << 16;

// Control2: per-block calibration enables.
inline constexpr std::uint32_t kControl2RxAdc            = 1u << 16;
inline constexpr std::uint32_t kControl2TxCtune          = 1u << 17;
inline constexpr std::uint32_t kControl2RxFilter         = 1u << 21;
inline constexpr std::uint32_t kControl2Calibration      = 0x33000000u;
inline constexpr std::uint32_t kControl2CalibrationClear = 0xFFCDFFFFu;

// Control3: start pulses for the RX filter/ADC state machines.
inline constexpr std::uint32_t kControl3RxFilterStart    = 1u << 4;
inline constexpr std::uint32_t kControl3RxAdcStart       = 1u << 8;
inline constexpr std::uint32_t kControl3CalPulse         = 1u << 12;

// TxBias: gates the TX bias during capacitance tuning.
inline constexpr std::uint32_t kTxBiasCtuneEnable        = 1u << 12;

// TxControl1: enables the TX path during capacitance tuning.
inline constexpr std::uint32_t kTxControl1TxEnable       = 1u << 4;
inline constexpr std::uint32_t kTxControl1ChannelBand    = 1u << 1;

// PhyConfig: PHY configuration field.
inline constexpr std::uint32_t kPhyConfigModeMask        = 0x700u;

// DacRef: nCO/nGA tuning results plus the tuning cycle count.
inline constexpr std::uint32_t kDacRefCoMask             = 0x3Fu;
inline constexpr std::uint32_t kDacRefCoShift            = 0;
inline constexpr std::uint32_t kDacRefTuneMask           = 0x1FFu;
inline constexpr std::uint32_t kDacRefTuneShift          = 8;
inline constexpr std::uint32_t kDacRefGaMask             = 0x7Fu;
inline constexpr std::uint32_t kDacRefGaShift            = 24;

// FilterControl: RX filter tuning code, bandwidth and enable.
inline constexpr std::uint32_t kFilterTuneMask           = 0x1Fu;
inline constexpr std::uint32_t kFilterBandwidthMask      = 0x7u << 8;
inline constexpr std::uint32_t kFilterBandwidth2M        = 0x3u << 8;
inline constexpr std::uint32_t kFilterEnable             = 1u << 16;

// AdcControl: RX ADC calibration gate.
inline constexpr std::uint32_t kAdcControlEnable         = 1u << 16;

// Calibration results.
inline constexpr std::uint32_t kTuneResultCoMask         = 0x3Fu;
inline constexpr std::uint32_t kTuneDone                 = 1u << 25;
inline constexpr std::uint32_t kTuneActive               = 1u << 26;
inline constexpr std::uint32_t kTuneResultGaMask         = 0x7Fu << 10;
inline constexpr std::uint32_t kTuneResultGaShift        = 10;
inline constexpr std::uint32_t kFilterResultDone         = 1u << 8;
inline constexpr std::uint32_t kFilterResultMask         = 0x1Fu;
} // namespace rfend

} // namespace radio_reg

} // namespace ch572
