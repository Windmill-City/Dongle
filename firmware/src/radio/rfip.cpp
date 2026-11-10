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
namespace lle                                      = radio_reg::lle;
namespace bb                                       = radio_reg::bb;
namespace rfend_reg                                = radio_reg::rfend;
namespace rfend_block                              = ch572::rfend;

ProcessCallback                g_process_cb        = nullptr;
std::uint32_t                  g_wait_recv_time    = 0;

// Vendor LLE PHY-control values for each radio mode.
inline constexpr std::uint32_t kLlePhyCalibration  = 0x000F05D8u;
inline constexpr std::uint32_t kLlePhyTx           = 0x000F02D8u;
inline constexpr std::uint32_t kLlePhyRx           = 0x000F01D8u;
inline constexpr std::uint32_t kLlePhyIdle         = 0x000F0080u;

// Vendor BB configuration values.
inline constexpr std::uint32_t kBbRxConfigValue    = 64u;
inline constexpr std::uint32_t kBbRxConfig2Value   = 0x20018u;
inline constexpr std::uint32_t kBbRxControl1Value  = 13u;
inline constexpr std::uint32_t kBbRxControl2Value  = 160u;
inline constexpr std::uint32_t kBbRxControl2High   = 127u;
inline constexpr std::uint32_t kBbReceiveDone      = 1u << 6;
inline constexpr std::uint32_t kBbTxPowerBoost     = 0x40u;
inline constexpr std::uint32_t kBbTxPowerStartMode = 1u;

// Sleep-clock gates cleared during radio init.
inline constexpr std::uint32_t kSleepClkOffBle     = 1u << 7;
inline constexpr std::uint32_t kSleepClkOff2Mask   = 0x13u;

// LLE wait time and fine-tune threshold.
inline constexpr std::uint32_t kRxWaitTime         = 4u;
inline constexpr std::uint32_t kTxFineTuneWait     = 40u;

// Vendor LLE duration formulas.
inline constexpr std::uint32_t kTxLengthShift      = 8;
inline constexpr std::uint32_t kTxLengthMask       = 0xFFu;
inline constexpr std::uint32_t kTxLengthOverhead   = 11u;
inline constexpr std::uint32_t kTxByteTicks        = 4u;
inline constexpr std::uint32_t kTxWaitDivisor      = 2u;
inline constexpr std::uint32_t kTxFixedTicks       = 44u;
inline constexpr std::uint32_t kLleTickScale       = 2u;
inline constexpr std::uint32_t kRxLengthOverhead   = 3u;
inline constexpr std::uint32_t kRxByteTicks        = 8u;
inline constexpr std::uint32_t kRxFixedTicks       = 60u;

CH572_HIGH_CODE void           rf_basic_process(std::uint32_t raw_irq) noexcept;
void                           wakeup_reg_init() noexcept;
void                           calibration() noexcept;
CH572_HIGH_CODE std::uint8_t read_crc() noexcept;

inline std::uint32_t         read_u32(const void* address) noexcept
{
    return *static_cast<const std::uint32_t*>(address);
}

CH572_HIGH_CODE void start_lle_timer(std::uint32_t total, std::uint32_t bit) noexcept
{
    const std::uint32_t saved = Lle::r32(lle::Config2);
    Lle::r32(lle::Config2)    = saved & ~bit;
    fence_i();
    Lle::r32(lle::Irq)     = bit;
    Lle::r32(lle::Timer)   = total;
    Lle::r32(lle::Config2) = saved;
}

CH572_HIGH_CODE void configure_channel(std::uint32_t channel) noexcept
{
    RfEnd::r32(rfend_reg::TxControl1) &= ~rfend_reg::kTxControl1ChannelBand;
    Bb::r32(bb::Control) = (Bb::r32(bb::Control) & ~(bb::kControlChannelMask | bb::kControlWhiteningOff))
                         | (channel & bb::kControlChannelMask);
}

CH572_HIGH_CODE void set_phy_tx_mode(std::uint32_t tx_dma) noexcept
{
    RfEnd::r32(rfend_reg::PhyConfig) &= ~rfend_reg::kPhyConfigModeMask;

    Bb::r32(bb::Control) &= ~bb::kControlPhyMask;
    const std::uint32_t dma_word = read_u32(reinterpret_cast<const void*>(tx_dma));
    Bb::r32(bb::Control3) &= ~bb::kControl3PhyMask;

    const std::uint32_t wait   = Lle::r16(lle::WaitTime);
    const std::uint32_t length = (dma_word >> kTxLengthShift) & kTxLengthMask;
    const std::uint32_t total =
        ((length + kTxLengthOverhead) * kTxByteTicks + (wait / kTxWaitDivisor + kTxFixedTicks)) * kLleTickScale;
    start_lle_timer(total, lle::kIrqTimer0);
}

CH572_HIGH_CODE void set_phy_rx_mode(std::uint8_t rx_max_len) noexcept
{
    Bb::r32(bb::Control) &= ~bb::kControlPhyMask;
    RfEnd::r32(rfend_reg::FilterControl) = (RfEnd::r32(rfend_reg::FilterControl) & ~rfend_reg::kFilterTuneMask)
                                         | (rfend_block::tune_filter_2m() & rfend_reg::kFilterTuneMask);
    Bb::r32(bb::Control3) &= ~bb::kControl3PhyMask;
    Bb::r32(bb::Control1)    = (Bb::r32(bb::Control1) & ~bb::kRxControl1Mask) | kBbRxControl1Value;
    Bb::r8(bb::Control2High) = kBbRxControl2High;
    Bb::r32(bb::Control2)    = (Bb::r32(bb::Control2) & ~bb::kRxControl2Mask) | kBbRxControl2Value;
    g_wait_recv_time = (static_cast<std::uint32_t>(rx_max_len) + kRxLengthOverhead) * kRxByteTicks + kRxFixedTicks;
}

CH572_HIGH_CODE void notify(RoleEvent event) noexcept
{
    if (g_process_cb != nullptr)
    {
        g_process_cb(event);
    }
}

CH572_HIGH_CODE void stop_lle_timer() noexcept
{
    Lle::r32(lle::Timer) = 0;
    Lle::r32(lle::Irq)   = lle::kIrqTimer4;
}

CH572_HIGH_CODE void rf_basic_process(std::uint32_t raw_irq) noexcept
{
    // A TX-done event suppresses the timeout event in the same status word.
    const bool tx_done = (raw_irq & lle::kIrqTxDone) != 0;
    if (tx_done)
    {
        stop_lle_timer();
        notify(kEventTx);
    }

    if ((raw_irq & lle::kIrqRx) != 0)
    {
        stop_lle_timer();
        notify((read_crc() == 0) ? kEventRx : kEventEr);
    }

    if (!tx_done && (raw_irq & lle::kIrqTimer0) != 0)
    {
        notify(kEventEr);
    }
}

} // namespace

void role_init(ProcessCallback process_cb) noexcept
{
    if (process_cb == nullptr)
    {
        return;
    }

    g_process_cb = process_cb;

    rfend_block::dev_init();

    wakeup_reg_init();

    calibration();

    enable_irq(Irq::BleBb);
    enable_irq(Irq::BleLle);
}

namespace
{

void wakeup_reg_init() noexcept
{
    {
        sysreg::SafeAccess access;
        sysreg::sleep_clk_off1() &= ~kSleepClkOffBle;
        sysreg::sleep_clk_off2() &= ~kSleepClkOff2Mask;
    }
    Lle::r32(lle::Config) |= lle::kConfigInit;
    Lle::r32(lle::Config54) = 0;
    Lle::r32(lle::Config2)  = lle::kConfig2Init;
    Bb::r32(bb::CrcStatus)  = bb::kCrcStatusClear;
    Bb::r32(bb::RxConfig)   = kBbRxConfigValue;
    Bb::r32(bb::RxConfig2)  = kBbRxConfig2Value;
}

void calibration() noexcept
{
    Bb::r32(bb::Control) = (Bb::r32(bb::Control) & ~bb::kControlModeMask) | bb::kControlCalibrationMode;
    RfEnd::r32(rfend_reg::Control2) |= rfend_reg::kControl2Calibration;
    Lle::r32(lle::PhyControl) = kLlePhyCalibration;

    rfend_block::calibration();

    Lle::r32(lle::Irq)   = lle::kIrqTimer0;
    Bb::r32(bb::Control) = (Bb::r32(bb::Control) & ~bb::kControlModeMask) | bb::kControlIdleMode;
    RfEnd::r32(rfend_reg::Control2) &= rfend_reg::kControl2CalibrationClear;
    Lle::r32(lle::PhyControl) = kLlePhyIdle;
}

std::uint8_t read_crc() noexcept
{
    return static_cast<std::uint8_t>((Bb::r32(bb::CrcStatus) >> bb::kCrcOkShift) & bb::kCrcOkMask);
}

} // namespace

void start_tx(Frame* frame) noexcept
{
    if (frame == nullptr)
    {
        return;
    }

    const std::uint32_t tx_dma = reinterpret_cast<std::uint32_t>(frame);
    Lle::r32(lle::Control) |= lle::kControlStop;

    const std::uint8_t tx_power = static_cast<std::uint8_t>(kLinkTxPower | kBbTxPowerBoost);
    Bb::r32(bb::TxPower)        = (Bb::r32(bb::TxPower) & ~bb::kTxPowerLevelMask)
                         | (static_cast<std::uint32_t>(tx_power) << bb::kTxPowerLevelShift);
    Lle::r16(lle::WaitTime) = kLinkWaitTime;
    Bb::r32(bb::TxPower)    = (Bb::r32(bb::TxPower) & ~bb::kTxPowerModeMask) | kBbTxPowerStartMode;
    Bb::r32(bb::Control)    = (Bb::r32(bb::Control) & ~bb::kControlModeMask) | bb::kControlCalibrationMode;
    RfEnd::r32(rfend_reg::Control2) |= rfend_reg::kControl2Calibration;
    Lle::r32(lle::PhyControl) = kLlePhyTx;

    configure_channel(kLinkFrequency);

    if (Lle::r16(lle::WaitTime) > kTxFineTuneWait)
    {
        RfEnd::r32(rfend_reg::Control1) &= ~rfend_reg::kControl1TxFine;
        nop();
        nop();
        nop();
        nop();
        RfEnd::r32(rfend_reg::Control1) |= rfend_reg::kControl1TxFine;
    }

    Bb::r32(bb::AccessAddress)   = kLinkAccessAddress;
    Bb::r32(bb::AccessAddressEx) = 0u;
    Bb::r32(bb::CrcInit)         = kLinkCrcInit;
    Bb::r32(bb::CrcPoly)         = (Bb::r32(bb::CrcPoly) & bb::kCrcPolyHighMask) | (kLinkCrcPoly & bb::kCrcPolyLowMask);
    Lle::r32(lle::TxDma)         = tx_dma;

    set_phy_tx_mode(tx_dma);

    Bb::r32(bb::Control) |= bb::kControlStartTx;
    Bb::r32(bb::TxPower) &= ~bb::kTxPowerModeMask;
    Lle::r32(lle::Control) = lle::kControlTx;
}

void set_rx(Frame* frame, std::uint32_t timeout) noexcept
{
    if (frame == nullptr)
    {
        return;
    }

    Lle::r16(lle::WaitTime) = kRxWaitTime;
    if ((Lle::r32(lle::Control) & lle::kControlModeMask) != 0)
    {
        Lle::r32(lle::Control) |= lle::kControlStop;
    }

    Bb::r32(bb::Control) = (Bb::r32(bb::Control) & ~bb::kControlModeMask) | bb::kControlCalibrationMode;
    RfEnd::r32(rfend_reg::Control2) |= rfend_reg::kControl2Calibration;
    Lle::r32(lle::PhyControl) = kLlePhyRx;

    configure_channel(kLinkFrequency);

    Bb::r32(bb::RxAccessAddress)   = kLinkAccessAddress;
    Bb::r32(bb::RxAccessAddressEx) = 0u;
    Bb::r32(bb::RxCrcInit)         = kLinkCrcInit;
    Bb::r32(bb::RxCrcPoly) = (Bb::r32(bb::RxCrcPoly) & bb::kCrcPolyHighMask) | (kLinkCrcPoly & bb::kCrcPolyLowMask);

    Lle::r32(lle::Timeout) = timeout;
    Lle::r32(lle::Timer)   = 0;
    Lle::r32(lle::Irq)     = lle::kIrqTimer4;

    set_phy_rx_mode(kMaxPacket);

    Lle::r32(lle::Control) = lle::kControlRx;

    if (timeout != 0)
    {
        start_lle_timer(timeout, lle::kIrqTimer4);
    }
}

void lle_irq_handler() noexcept
{
    const std::uint32_t status = Lle::r32(lle::Irq);
    const std::uint32_t enable = Lle::r32(lle::Config2);
    if ((status & enable) == 0)
    {
        return;
    }

    Lle::r32(lle::Irq) = status;
    rf_basic_process(status & enable);
}

void bb_irq_handler() noexcept
{
    const std::uint32_t control = Bb::r32(bb::Control);
    if ((control & kBbReceiveDone) != 0)
    {
        Bb::r32(bb::CrcStatus) = bb::kCrcStatusRxAck;
        Lle::r32(lle::Irq)     = lle::kIrqTimer4;
        Lle::r32(lle::Timer)   = g_wait_recv_time;
    }
}

} // namespace ch572::rf

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
