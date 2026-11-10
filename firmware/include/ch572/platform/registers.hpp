#pragma once

#include <cstddef>
#include <cstdint>

namespace ch572
{

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
    std::uint32_t          reserved0[2];
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
    volatile std::uint8_t  iprior[256];
    std::uint8_t           reserved8[0x810];
    volatile std::uint32_t sctlr;
};

inline Pfic& pfic() noexcept
{
    return *reinterpret_cast<Pfic*>(0xE000E000u);
}

inline void enable_irq(Irq irq) noexcept
{
    const auto index   = static_cast<std::uint32_t>(irq) >> 5;
    const auto bit     = 1u << (static_cast<std::uint32_t>(irq) & 0x1Fu);
    pfic().ienr[index] = bit;
}

// ---------------------------------------------------------------------------
// System control registers
// ---------------------------------------------------------------------------

namespace sysreg
{

inline volatile std::uint8_t& safe_access_sig() noexcept
{
    return *reinterpret_cast<volatile std::uint8_t*>(0x40001040u);
}

inline volatile std::uint8_t& clock_sys_cfg() noexcept
{
    return *reinterpret_cast<volatile std::uint8_t*>(0x40001008u);
}

inline volatile std::uint8_t& hfck_power_ctrl() noexcept
{
    return *reinterpret_cast<volatile std::uint8_t*>(0x4000100Au);
}

inline volatile std::uint8_t& sleep_power_ctrl() noexcept
{
    return *reinterpret_cast<volatile std::uint8_t*>(0x4000100Fu);
}

inline volatile std::uint8_t& xt32m_tune() noexcept
{
    return *reinterpret_cast<volatile std::uint8_t*>(0x4000104Eu);
}

inline volatile std::uint8_t& flash_cfg() noexcept
{
    return *reinterpret_cast<volatile std::uint8_t*>(0x40001807u);
}

inline volatile std::uint8_t& flash_sck() noexcept
{
    return *reinterpret_cast<volatile std::uint8_t*>(0x40001805u);
}

inline volatile std::uint32_t& misc_ctrl() noexcept
{
    return *reinterpret_cast<volatile std::uint32_t*>(0x40001048u);
}

inline volatile std::uint8_t& sleep_clk_off1() noexcept
{
    return *reinterpret_cast<volatile std::uint8_t*>(0x4000100Du);
}

inline volatile std::uint8_t& sleep_clk_off2() noexcept
{
    return *reinterpret_cast<volatile std::uint8_t*>(0x4000101Du);
}

inline volatile std::uint8_t& reset_watchdog_ctrl() noexcept
{
    return *reinterpret_cast<volatile std::uint8_t*>(0x40001046u);
}

inline volatile std::uint16_t& pin_alternate() noexcept
{
    return *reinterpret_cast<volatile std::uint16_t*>(0x40001018u);
}

inline constexpr std::uint8_t  kSafeAccessSig1  = 0x57;
inline constexpr std::uint8_t  kSafeAccessSig2  = 0xA8;
inline constexpr std::uint8_t  kClockPllOn      = 0x10;
inline constexpr std::uint8_t  kClockXt32mOn    = 0x04;
inline constexpr std::uint16_t kPinDebugEnable  = 0x4000;
inline constexpr std::uint16_t kPinUsbEnable    = 0x2000;
inline constexpr std::uint16_t kUdpPullUpEnable = 0x1000;
inline constexpr std::uint8_t  kSoftwareReset   = 0x01;

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
        safe_access_sig() = 0;
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
    reset_watchdog_ctrl() &= ~0x06u;
}

} // namespace sysreg

// ---------------------------------------------------------------------------
// USB device controller
// ---------------------------------------------------------------------------

namespace usbreg
{

inline constexpr std::uintptr_t kBase = 0x40008000u;

template <typename T>
inline volatile T& at(std::size_t offset) noexcept
{
    return *reinterpret_cast<volatile T*>(kBase + offset);
}

inline volatile std::uint8_t& ctrl() noexcept
{
    return at<std::uint8_t>(0x00);
}

inline volatile std::uint8_t& dev_ctrl() noexcept
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

inline volatile std::uint8_t& ep0_t_len() noexcept
{
    return at<std::uint8_t>(0x20);
}

inline volatile std::uint8_t& ep0_ctrl() noexcept
{
    return at<std::uint8_t>(0x22);
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

inline constexpr std::uint8_t kUcDevPuEn    = 0x20;
inline constexpr std::uint8_t kUcIntBusy    = 0x08;
inline constexpr std::uint8_t kUcDmaEn      = 0x01;
inline constexpr std::uint8_t kUdPdDis      = 0x80;
inline constexpr std::uint8_t kUdPortEn     = 0x01;
inline constexpr std::uint8_t kUieSuspend   = 0x04;
inline constexpr std::uint8_t kUieTransfer  = 0x02;
inline constexpr std::uint8_t kUieBusReset  = 0x01;
inline constexpr std::uint8_t kUifTransfer  = 0x02;
inline constexpr std::uint8_t kUifBusReset  = 0x01;
inline constexpr std::uint8_t kUifSuspend   = 0x04;
inline constexpr std::uint8_t kUisSetupAct  = 0x80;
inline constexpr std::uint8_t kMaskUisToken = 0x30;
inline constexpr std::uint8_t kUep1TxEn     = 0x40;
inline constexpr std::uint8_t kUep2RxEn     = 0x08;
inline constexpr std::uint8_t kUep2TxEn     = 0x04;
inline constexpr std::uint8_t kUepRTog      = 0x80;
inline constexpr std::uint8_t kUepTTog      = 0x40;
inline constexpr std::uint8_t kMaskUepRRes  = 0x0C;
inline constexpr std::uint8_t kUepRResAck   = 0x00;
inline constexpr std::uint8_t kUepRResNak   = 0x08;
inline constexpr std::uint8_t kUepRResStall = 0x0C;
inline constexpr std::uint8_t kMaskUepTRes  = 0x03;
inline constexpr std::uint8_t kUepTResAck   = 0x00;
inline constexpr std::uint8_t kUepTResNak   = 0x02;
inline constexpr std::uint8_t kUepTResStall = 0x03;

} // namespace usbreg

// ---------------------------------------------------------------------------
// 2.4 GHz radio register windows
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
        return *reinterpret_cast<volatile std::uint32_t*>(Base + offset);
    }

    static volatile std::uint16_t& r16(std::size_t offset) noexcept
    {
        return *reinterpret_cast<volatile std::uint16_t*>(Base + offset);
    }

    static volatile std::uint8_t& r8(std::size_t offset) noexcept
    {
        return *reinterpret_cast<volatile std::uint8_t*>(Base + offset);
    }
};

using Lle   = Window<kLleBase>;
using Bb    = Window<kBbBase>;
using RfEnd = Window<kRfEndBase>;

namespace lle
{
inline constexpr std::size_t Control    = 0x00;
inline constexpr std::size_t Config     = 0x04;
inline constexpr std::size_t WaitTime   = 0x06;
inline constexpr std::size_t Irq        = 0x08;
inline constexpr std::size_t Config2    = 0x0C;
inline constexpr std::size_t PhyControl = 0x50;
inline constexpr std::size_t Timer      = 0x64;
inline constexpr std::size_t TxDma      = 0x78;
inline constexpr std::size_t Timeout    = 0x7C;
inline constexpr std::size_t Config54   = 0x54;

// Raw interrupt bits. The hardware names come from the five LLE timers; the
// radio driver uses timer 0 as the RX timeout and timer 4 as the generic timer.
inline constexpr std::uint32_t kIrqRx     = 1u << 0;
inline constexpr std::uint32_t kIrqTxDone = 1u << 2;
inline constexpr std::uint32_t kIrqTimer0 = 1u << 17;
inline constexpr std::uint32_t kIrqTimer4 = 1u << 21;
} // namespace lle

namespace bb
{
inline constexpr std::size_t Control           = 0x00;
inline constexpr std::size_t CrcInit           = 0x04;
inline constexpr std::size_t CrcPoly           = 0x08;
inline constexpr std::size_t AccessAddress     = 0x0C;
inline constexpr std::size_t AccessAddressEx   = 0x10;
inline constexpr std::size_t Control1          = 0x14;
inline constexpr std::size_t Control2High      = 0x1E;
inline constexpr std::size_t Control2          = 0x1C;
inline constexpr std::size_t Control3          = 0x24;
inline constexpr std::size_t TxPower           = 0x34;
inline constexpr std::size_t RxConfig          = 0x3C;
inline constexpr std::size_t CrcStatus         = 0x40;
inline constexpr std::size_t RxCrcInit         = 0x58;
inline constexpr std::size_t RxCrcPoly         = 0x5C;
inline constexpr std::size_t RxAccessAddress   = 0x60;
inline constexpr std::size_t RxAccessAddressEx = 0x64;

// Control register fields.
inline constexpr std::uint32_t kControlChannelMask  = 0x3Fu;
inline constexpr std::uint32_t kControlWhiteningOff = 1u << 6;
} // namespace bb

// RFEND (0x4000D000) is absent from the CH572 datasheet and SVD. The offsets
// and fields below were cross-checked against two reverse-engineering sources:
//   - cnlohr/ch32fun, extralibs/iSLER.h (covers CH570/CH572)
//   - ArcaneNibble/project-nanjing (CH579 RFEND register notes)
// Names are descriptive; WCH does not publish vendor names for this block.
namespace rfend
{
// Register offsets.
inline constexpr std::size_t Control1      = 0x04;
inline constexpr std::size_t Control2      = 0x08;
inline constexpr std::size_t Control3      = 0x0C;
inline constexpr std::size_t TxBias        = 0x28;
inline constexpr std::size_t TxControl1    = 0x2C;
inline constexpr std::size_t TxControl2    = 0x30;
inline constexpr std::size_t PhyConfig     = 0x34;
inline constexpr std::size_t DacRef        = 0x38;
inline constexpr std::size_t Power         = 0x3C;
inline constexpr std::size_t TuneControl   = 0x48;
inline constexpr std::size_t FilterControl = 0x50;
inline constexpr std::size_t AdcControl    = 0x58;
inline constexpr std::size_t RxControl     = 0x5C;
inline constexpr std::size_t TuneResult    = 0x90;
inline constexpr std::size_t TuneResultGa  = 0x94;
inline constexpr std::size_t FilterResult  = 0x9C;
inline constexpr std::size_t TuneTableCo   = 0xA0;
inline constexpr std::size_t TuneTableGa   = 0xC8;

// Control1: one strobe per calibration step.
inline constexpr std::uint32_t kControl1TuneStart = 1u << 0;
inline constexpr std::uint32_t kControl1TxCoarse  = 1u << 4;
inline constexpr std::uint32_t kControl1TxFine    = 1u << 8;
inline constexpr std::uint32_t kControl1RxFilter  = 1u << 12;
inline constexpr std::uint32_t kControl1RxAdc     = 1u << 16;

// Control2: per-block calibration enables.
inline constexpr std::uint32_t kControl2RxAdc    = 1u << 16;
inline constexpr std::uint32_t kControl2TxCtune  = 1u << 17;
inline constexpr std::uint32_t kControl2RxFilter = 1u << 21;

// Control3: start pulses for the RX filter/ADC state machines.
inline constexpr std::uint32_t kControl3RxFilterStart = 1u << 4;
inline constexpr std::uint32_t kControl3RxAdcStart    = 1u << 8;
inline constexpr std::uint32_t kControl3CalPulse      = 1u << 12;

// TxBias: gates the TX bias during capacitance tuning.
inline constexpr std::uint32_t kTxBiasCtuneEnable = 1u << 12;

// TxControl1: enables the TX path during capacitance tuning.
inline constexpr std::uint32_t kTxControl1TxEnable = 1u << 4;

// DacRef: nCO/nGA tuning results plus the tuning cycle count.
inline constexpr std::uint32_t kDacRefCoMask    = 0x3Fu;
inline constexpr std::uint32_t kDacRefCoShift   = 0;
inline constexpr std::uint32_t kDacRefTuneMask  = 0x1FFu;
inline constexpr std::uint32_t kDacRefTuneShift = 8;
inline constexpr std::uint32_t kDacRefGaMask    = 0x7Fu;
inline constexpr std::uint32_t kDacRefGaShift   = 24;

// FilterControl: RX filter tuning code, bandwidth and enable.
inline constexpr std::uint32_t kFilterTuneMask      = 0x1Fu;
inline constexpr std::uint32_t kFilterBandwidthMask = 0x7u << 8;
inline constexpr std::uint32_t kFilterBandwidth2M   = 0x3u << 8;
inline constexpr std::uint32_t kFilterEnable        = 1u << 16;

// AdcControl: RX ADC calibration gate.
inline constexpr std::uint32_t kAdcControlEnable = 1u << 16;

// Calibration results.
inline constexpr std::uint32_t kTuneResultCoMask  = 0x3Fu;
inline constexpr std::uint32_t kTuneDoneMask      = (1u << 25) | (1u << 26);
inline constexpr std::uint32_t kTuneResultGaMask  = 0x7Fu << 10;
inline constexpr std::uint32_t kTuneResultGaShift = 10;
inline constexpr std::uint32_t kFilterResultDone  = 1u << 8;
inline constexpr std::uint32_t kFilterResultMask  = 0x1Fu;
} // namespace rfend

} // namespace radio_reg

} // namespace ch572
