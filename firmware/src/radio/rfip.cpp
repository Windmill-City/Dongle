#include "ch572/radio/rfip.hpp"

#include "ch572/compiler.hpp"
#include "ch572/platform/registers.hpp"
#include "ch572/radio/rfend.hpp"

namespace ch572::rf
{

namespace
{

using radio_reg::Bb;
using radio_reg::Lle;
using radio_reg::RfEnd;

namespace lle                                    = radio_reg::lle;
namespace bb                                     = radio_reg::bb;
namespace reg                                    = radio_reg::rfend;

ProcessCallback                g_callback        = nullptr;

Frame*                         g_rx              = nullptr;

inline constexpr std::uint32_t kProperties       = 0x10u;

inline constexpr std::uint32_t kPhyCalibration   = 0x000F05D8u;
inline constexpr std::uint32_t kPhyTx            = 0x000F02D8u;
inline constexpr std::uint32_t kPhyRx            = 0x000F01D8u;
inline constexpr std::uint32_t kPhyIdle          = 0x000F0080u;

inline constexpr std::uint32_t kBleClkGate1      = 1u << 7;
inline constexpr std::uint32_t kBleClkGate2      = 0x13u;

inline constexpr std::uint32_t kBbRxConfig       = 64u;
inline constexpr std::uint32_t kBbRxConfig2      = 0x2071Fu;

inline constexpr std::uint32_t kCalEnable        = 0x00130000u;
inline constexpr std::uint32_t kCalClear         = 0xFFCDFFFFu;
inline constexpr std::uint32_t kTxBiasCal        = 1u << 11;
inline constexpr std::uint32_t kControl3Cal      = 1u << 12;

inline constexpr std::uint32_t kChannelMask      = 0x7Fu;

inline constexpr std::uint32_t kBbRxControl1     = 13u;
inline constexpr std::uint32_t kBbRxControl2     = 160u;
inline constexpr std::uint32_t kBbRxControl2High = 127u;
inline constexpr std::uint32_t kRxWaitTime       = 4u;

inline constexpr std::uint32_t kFrameLengthShift = 8u;
inline constexpr std::uint32_t kFrameLengthMask  = 0xFFu;

inline constexpr std::uint32_t kTxLengthOverhead = 11u;
inline constexpr std::uint32_t kTxByteTicks      = 4u;
inline constexpr std::uint32_t kTxFixedTicks     = 80u;
inline constexpr std::uint32_t kTxFineTuneWait   = 40u;

inline constexpr std::uint32_t kRxLengthOverhead = 3u;
inline constexpr std::uint32_t kRxByteTicks      = 8u;
inline constexpr std::uint32_t kRxFixedTicks     = 60u;

inline constexpr std::uint32_t kMaxRecvTime =
    (static_cast<std::uint32_t>(kMaxPacket) + kRxLengthOverhead) * kRxByteTicks + kRxFixedTicks;

inline constexpr int kInitSettleLoops = 100;
inline constexpr int kPhySettleLoops  = 40;

CH572_HIGH_CODE void rf_basic_process(std::uint32_t status) noexcept;
void                 wakeup() noexcept;
void                 calibration() noexcept;

CH572_HIGH_CODE std::uint8_t read_crc() noexcept
{
    return static_cast<std::uint8_t>((Bb::r32(bb::CrcStatus) >> bb::kCrcOkShift) & bb::kCrcOkMask);
}

CH572_HIGH_CODE void stop_lle_timer() noexcept
{
    Lle::r32(lle::Timer) = 0;
    Lle::r32(lle::Irq)   = lle::kIrqTimer0;
}

CH572_HIGH_CODE void program_channel() noexcept
{
    RfEnd::r32(reg::TxControl1) &= ~reg::kTxControl1ChannelBand;
    Bb::r32(bb::Control) = (Bb::r32(bb::Control) & ~kChannelMask) | (kLinkFrequency & kChannelMask);
}

CH572_HIGH_CODE void set_phy_tx_mode(const Frame* frame) noexcept
{
    RfEnd::r32(reg::PhyConfig) =
        (RfEnd::r32(reg::PhyConfig) & ~reg::kPhyConfigModeMask) | (kProperties & reg::kPhyConfigModeMask);

    Bb::r32(bb::Control) &= ~bb::kControlPhyMask;
    Bb::r32(bb::Control3) &= ~bb::kControl3PhyMask;

    const std::uint32_t dma_word = *reinterpret_cast<const volatile std::uint32_t*>(frame);
    const std::uint32_t length   = (dma_word >> kFrameLengthShift) & kFrameLengthMask;
    const std::uint32_t wait     = Lle::r16(lle::WaitTime);
    const std::uint32_t total    = ((length + kTxLengthOverhead) * kTxByteTicks + (wait / 2u + kTxFixedTicks)) * 2u;

    Lle::r32(lle::Config2) &= ~lle::kIrqTimer0;
    Lle::r32(lle::Irq)   = lle::kIrqTimer0;
    Lle::r32(lle::Timer) = total;
}

CH572_HIGH_CODE void set_phy_rx_mode() noexcept
{
    Bb::r32(bb::Control) &= ~bb::kControlPhyMask;
    RfEnd::r32(reg::FilterControl) =
        (RfEnd::r32(reg::FilterControl) & ~reg::kFilterTuneMask) | (rfend::tune_filter_2m() & reg::kFilterTuneMask);
    Bb::r32(bb::Control3) &= ~bb::kControl3PhyMask;

    Bb::r32(bb::Control1)    = (Bb::r32(bb::Control1) & ~bb::kRxControl1Mask) | kBbRxControl1;
    Bb::r8(bb::Control2High) = static_cast<std::uint8_t>(kBbRxControl2High);
    Bb::r32(bb::Control2)    = (Bb::r32(bb::Control2) & ~bb::kRxControl2Mask) | kBbRxControl2;
}

CH572_HIGH_CODE void rf_basic_process(std::uint32_t status) noexcept
{
    if ((status & lle::kIrqTx) != 0)
    {
        stop_lle_timer();
        Lle::r32(lle::Config2) |= lle::kIrqTimer0;
        g_callback(kEventTx);
        return;
    }

    if ((status & lle::kIrqRx) != 0)
    {
        stop_lle_timer();
        if (read_crc() == 0)
        {
            g_callback(kEventRx);
        }
        else
        {
            set_rx(g_rx);
        }
    }
    else if ((status & lle::kIrqTimer0) != 0)
    {
        set_rx(g_rx);
    }
}

}

void init(ProcessCallback process_cb) noexcept
{
    if (process_cb == nullptr)
    {
        return;
    }

    g_callback = process_cb;

    {
        sysreg::SafeAccess access;
        sysreg::misc_ctrl() = (sysreg::misc_ctrl() & ~sysreg::kRfAnalogMask) | sysreg::kRfAnalogEnable;
    }

    rfend::init();
    wakeup();
    calibration();

    enable_irq(Irq::BleBb);
    enable_irq(Irq::BleLle);
}

namespace
{

void wakeup() noexcept
{
    {
        sysreg::SafeAccess access;
        sysreg::sleep_clk_off1() &= ~kBleClkGate1;
        sysreg::sleep_clk_off2() &= ~kBleClkGate2;
    }

    Lle::r32(lle::Config) |= lle::kConfigInit;
    Lle::r32(lle::Config54) = 0;
    Lle::r32(lle::Config2)  = lle::kConfig2Init;
    Bb::r32(bb::CrcStatus)  = bb::kCrcStatusClear;
    Bb::r32(bb::RxConfig)   = kBbRxConfig;
    Bb::r32(bb::RxConfig2)  = kBbRxConfig2;
}

void calibration() noexcept
{
    Bb::r32(bb::Control) = (Bb::r32(bb::Control) & ~bb::kControlModeMask) | bb::kControlCalibMode;
    RfEnd::r32(reg::Control2) |= kCalEnable;
    RfEnd::r32(reg::TxBias) |= kTxBiasCal;
    Lle::r32(lle::PhyControl) = kPhyCalibration;

    for (volatile int loop = 0; loop < kPhySettleLoops; ++loop)
    {
    }

    RfEnd::r32(reg::Control3) |= kControl3Cal;
    RfEnd::r32(reg::Control3) &= ~kControl3Cal;
    nop();
    nop();
    nop();
    nop();
    RfEnd::r32(reg::Control3) |= kControl3Cal;
    RfEnd::r32(reg::TxBias) &= ~kTxBiasCal;

    rfend::calibration();

    Lle::r32(lle::Irq)   = lle::kIrqTimer0;
    Bb::r32(bb::Control) = (Bb::r32(bb::Control) & ~bb::kControlModeMask) | bb::kControlIdleMode;
    RfEnd::r32(reg::Control2) &= kCalClear;
    Lle::r32(lle::PhyControl) = kPhyIdle;
}

}

void set_tx(Frame* frame) noexcept
{
    if (frame == nullptr)
    {
        return;
    }

    Lle::r32(lle::Control) |= lle::kControlStop;

    Lle::r32(lle::Irq)          = lle::kIrqRx | lle::kIrqTx | lle::kIrqTimer0;
    Bb::r32(bb::CrcStatus)      = bb::kCrcStatusClear;

    const std::uint8_t tx_power = static_cast<std::uint8_t>(kLinkTxPower | bb::kTxPowerLevelFlag);
    Bb::r32(bb::TxPower)        = (Bb::r32(bb::TxPower) & bb::kTxPowerLowFieldMask)
                                | (static_cast<std::uint32_t>(tx_power) << bb::kTxPowerLevelShift);
    Lle::r16(lle::WaitTime)     = static_cast<std::uint16_t>(kLinkWaitTime);
    Bb::r32(bb::TxPower)        = (Bb::r32(bb::TxPower) & ~bb::kTxPowerModeMask) | bb::kTxPowerModeCalibration;

    Bb::r32(bb::Control)        = Bb::r32(bb::Control) & ~bb::kControlModeMask;
    Bb::r32(bb::Control)        = (Bb::r32(bb::Control) & ~bb::kControlModeMask) | bb::kControlCalibMode;

    RfEnd::r32(reg::Control2) |= kCalEnable;
    RfEnd::r32(reg::TxBias) |= kTxBiasCal;
    Lle::r32(lle::PhyControl) = kPhyTx;

    for (volatile int loop = 0; loop < kPhySettleLoops; ++loop)
    {
    }

    RfEnd::r32(reg::Control3) |= kControl3Cal;
    RfEnd::r32(reg::Control3) &= ~kControl3Cal;
    nop();
    nop();
    nop();
    nop();
    RfEnd::r32(reg::Control3) |= kControl3Cal;
    RfEnd::r32(reg::TxBias) &= ~kTxBiasCal;

    program_channel();

    if (Lle::r16(lle::WaitTime) > kTxFineTuneWait)
    {
        RfEnd::r32(reg::Control1) &= ~reg::kControl1TxFine;
        nop();
        nop();
        nop();
        nop();
        RfEnd::r32(reg::Control1) |= reg::kControl1TxFine;
    }

    Bb::r32(bb::AccessAddress)   = kLinkAccessAddress;
    Bb::r32(bb::AccessAddressEx) = 0u;
    Bb::r32(bb::CrcInit)         = kLinkCrcInit;
    Bb::r32(bb::CrcPoly)         = (Bb::r32(bb::CrcPoly) & bb::kCrcPolyHighMask) | (kLinkCrcPoly & bb::kCrcPolyLowMask);

    Lle::r32(lle::TxDma)         = reinterpret_cast<std::uint32_t>(frame);

    set_phy_tx_mode(frame);

    Bb::r32(bb::Control) |= bb::kControlStartTx;
    Bb::r32(bb::TxPower) &= ~bb::kTxPowerModeMask;
    Lle::r32(lle::Control) = lle::kControlTx;
}

void set_rx(Frame* frame) noexcept
{
    if (frame == nullptr)
    {
        return;
    }

    g_rx                    = frame;

    Lle::r16(lle::WaitTime) = static_cast<std::uint16_t>(kRxWaitTime);
    if ((Lle::r32(lle::Control) & lle::kControlModeMask) != 0)
    {
        Lle::r32(lle::Control) |= lle::kControlStop;
    }

    Bb::r32(bb::Control) = Bb::r32(bb::Control) & ~bb::kControlModeMask;
    Bb::r32(bb::Control) = (Bb::r32(bb::Control) & ~bb::kControlModeMask) | bb::kControlCalibMode;

    RfEnd::r32(reg::Control2) |= kCalEnable;
    RfEnd::r32(reg::TxBias) |= kTxBiasCal;
    Lle::r32(lle::PhyControl) = kPhyRx;

    for (volatile int loop = 0; loop < kPhySettleLoops; ++loop)
    {
    }

    RfEnd::r32(reg::Control3) |= kControl3Cal;
    RfEnd::r32(reg::Control3) &= ~kControl3Cal;
    nop();
    nop();
    nop();
    nop();
    RfEnd::r32(reg::Control3) |= kControl3Cal;
    RfEnd::r32(reg::TxBias) &= ~kTxBiasCal;

    program_channel();

    Bb::r32(bb::RxAccessAddress)   = kLinkAccessAddress;
    Bb::r32(bb::RxAccessAddressEx) = 0u;
    Bb::r32(bb::RxCrcInit)         = kLinkCrcInit;
    Bb::r32(bb::RxCrcPoly) = (Bb::r32(bb::RxCrcPoly) & bb::kCrcPolyHighMask) | (kLinkCrcPoly & bb::kCrcPolyLowMask);

    Lle::r32(lle::RxDma)   = reinterpret_cast<std::uint32_t>(frame);
    Lle::r32(lle::Timer)   = 0;
    Lle::r32(lle::Irq)     = lle::kIrqTimer0;

    set_phy_rx_mode();

    Lle::r32(lle::Control) = lle::kControlRx;
}

void lle_irq_handler() noexcept
{
    const std::uint32_t irq  = Lle::r32(lle::Irq);
    const std::uint32_t mask = Lle::r32(lle::Config2);
    if ((irq & mask) == 0)
    {
        return;
    }

    Lle::r32(lle::Irq) = irq;
    rf_basic_process(irq & mask);
}

void bb_irq_handler() noexcept
{
    if ((Bb::r32(bb::CrcStatus) & bb::kCrcStatusDone) != 0)
    {
        Bb::r32(bb::CrcStatus) = bb::kCrcStatusRxAck;
        Lle::r32(lle::Irq)     = lle::kIrqTimer0;
        Lle::r32(lle::Timer)   = kMaxRecvTime;
    }
}

}

extern "C"
{
CH572_INTERRUPT CH572_HIGH_CODE void LLE_IRQHandler(void)
{
    ch572::rf::lle_irq_handler();
}

CH572_INTERRUPT CH572_HIGH_CODE void BB_IRQHandler(void)
{
    ch572::rf::bb_irq_handler();
}
}
