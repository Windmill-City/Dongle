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
namespace lle                                   = radio_reg::lle;
namespace bb                                    = radio_reg::bb;
namespace rfend_reg                             = radio_reg::rfend;
namespace rfend_block                           = ch572::rfend;

ProcessCallback                g_process_cb     = nullptr;
std::uint32_t                  g_wait_recv_time = 0;

// Raw LLE interrupt bits. Bit 2 means "TX finished" in the LLE status register;
// the timeout is bit 17. Keep the decoding here so the public RoleEvent values
// stay independent of the vendor register layout.
inline constexpr std::uint32_t kLleRx           = 1u << 0;
inline constexpr std::uint32_t kLleTxDone       = 1u << 2;
inline constexpr std::uint32_t kLleTimeout      = 1u << 17;
inline constexpr std::uint32_t kLleTimerIrq     = 0x200000u;

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
    RfEnd::r32(rfend_reg::TxControl) &= ~2u;
    Bb::r32(bb::Control) = (Bb::r32(bb::Control) & 0xFFFFFF80u) | (channel & 0x7Fu);
}

CH572_HIGH_CODE void set_phy_tx_mode(std::uint32_t tx_dma) noexcept
{
    RfEnd::r32(rfend_reg::PhyConfig) &= ~0x700u;

    Bb::r32(bb::Control) &= ~0x300u;
    const std::uint32_t dma_word = read_u32(reinterpret_cast<const void*>(tx_dma));
    Bb::r32(bb::Control3) &= 0xF9FFFFFFu;

    const std::uint32_t wait   = Lle::r16(lle::WaitTime);
    const std::uint32_t length = (dma_word >> 8) & 0xFFu;
    const std::uint32_t total  = (((length + 11u) << 2) + ((wait >> 1) + 44u)) << 1;
    start_lle_timer(total, 0x20000u);
}

CH572_HIGH_CODE void set_phy_rx_mode(std::uint8_t rx_max_len) noexcept
{
    Bb::r32(bb::Control) &= ~0x300u;
    RfEnd::r32(rfend_reg::FilterControl) =
        (RfEnd::r32(rfend_reg::FilterControl) & ~0x1Fu) | (rfend_block::tune_filter_2m() & 0x1Fu);
    Bb::r32(bb::Control3) &= 0xF9FFFFFFu;
    Bb::r32(bb::Control1)    = (Bb::r32(bb::Control1) & ~0x3Fu) | 13u;
    Bb::r8(bb::Control2High) = 127u;
    Bb::r32(bb::Control2)    = (Bb::r32(bb::Control2) & ~0x3FFu) | 160u;
    g_wait_recv_time         = ((static_cast<std::uint32_t>(rx_max_len) + 3u) << 3) + 60u;
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
    Lle::r32(lle::Irq)   = kLleTimerIrq;
}

CH572_HIGH_CODE void rf_basic_process(std::uint32_t raw_irq) noexcept
{
    // A TX-done event suppresses the timeout event in the same status word.
    const bool tx_done = (raw_irq & kLleTxDone) != 0;
    if (tx_done)
    {
        stop_lle_timer();
        notify(kEventTxFinish);
    }

    if ((raw_irq & kLleRx) != 0)
    {
        stop_lle_timer();
        notify((read_crc() == 0) ? kEventRx : kEventCrcError);
    }

    if (!tx_done && (raw_irq & kLleTimeout) != 0)
    {
        notify(kEventTimeout);
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
    sysreg::iwdg_feed();

    rfend_block::dev_init();
    sysreg::iwdg_feed();

    wakeup_reg_init();
    sysreg::iwdg_feed();

    calibration();
    sysreg::iwdg_feed();

    enable_irq(Irq::BleBb);
    enable_irq(Irq::BleLle);
}

namespace
{

void wakeup_reg_init() noexcept
{
    {
        sysreg::SafeAccess access;
        sysreg::sleep_clk_off1() &= ~0x80u;
        sysreg::sleep_clk_off2() &= ~0x13u;
    }
    Lle::r32(lle::Config) |= 0x1Eu;
    Lle::r32(lle::Config54) = 0;
    Lle::r32(lle::Config2)  = 0x20003u;
    Bb::r32(bb::CrcStatus)  = 0xFFFFu;
    Bb::r32(bb::RxConfig)   = 64u;
    Bb::r32(0x38)           = 0x20018u;
}

void calibration() noexcept
{
    Bb::r32(bb::Control) = (Bb::r32(bb::Control) & 0xFFFCFFFFu) | 0x200000u;
    RfEnd::r32(rfend_reg::Control2) |= 0x33000000u;
    RfEnd::r32(rfend_reg::TxBias) |= 0x00000800u;
    Lle::r32(lle::PhyControl) = 0x000F05D8u;

    for (int i = 0; i < 40; ++i)
    {
        nop();
    }

    RfEnd::r32(rfend_reg::Control3) |= 0x00001000u;
    RfEnd::r32(rfend_reg::Control3) &= ~0x00001000u;
    nop();
    nop();
    nop();
    nop();
    RfEnd::r32(rfend_reg::Control3) |= 0x00001000u;
    RfEnd::r32(rfend_reg::TxBias) &= ~0x00000800u;

    rfend_block::calibration();

    Lle::r32(lle::Irq)   = 0x20000u;
    Bb::r32(bb::Control) = (Bb::r32(bb::Control) & 0xFFFCFFFFu) | 0x10000u;
    RfEnd::r32(rfend_reg::Control2) &= 0xFFCDFFFFu;
    Lle::r32(lle::PhyControl) = 0x000F0080u;
}

std::uint8_t read_crc() noexcept
{
    return static_cast<std::uint8_t>((Bb::r32(bb::CrcStatus) >> 5) & 1u);
}

} // namespace

void start_tx(Frame* frame) noexcept
{
    if (frame == nullptr)
    {
        return;
    }

    const std::uint32_t tx_dma = reinterpret_cast<std::uint32_t>(frame);
    Lle::r32(lle::Control) |= 8u;
    Bb::r32(bb::TxPower) =
        (Bb::r32(bb::TxPower) & 0x01FFFFFFu)
        | (static_cast<std::uint32_t>((static_cast<std::uint8_t>(kLinkTxPower) | 0x40u) & 0xFFu) << 25);
    Lle::r16(lle::WaitTime) = kLinkWaitTime;
    Bb::r32(bb::TxPower)    = (Bb::r32(bb::TxPower) & ~3u) | 1u;
    Bb::r32(bb::Control)    = (Bb::r32(bb::Control) & 0xFFFCFFFFu) | 0x200000u;
    RfEnd::r32(rfend_reg::Control2) |= 0x33000000u;
    Lle::r32(lle::PhyControl) = 0x000F02D8u;

    configure_channel(kLinkFrequency);

    if (Lle::r16(lle::WaitTime) > 40u)
    {
        RfEnd::r32(rfend_reg::Control) &= ~0x100u;
        nop();
        nop();
        nop();
        nop();
        RfEnd::r32(rfend_reg::Control) |= 0x100u;
    }

    Bb::r32(bb::AccessAddress)   = kLinkAccessAddress;
    Bb::r32(bb::AccessAddressEx) = 0u;
    Bb::r32(bb::CrcInit)         = kLinkCrcInit;
    Bb::r32(bb::CrcPoly)         = (Bb::r32(bb::CrcPoly) & 0xFF000000u) | (kLinkCrcPoly & 0x00FFFFFFu);
    Lle::r32(lle::TxDma)         = tx_dma;

    set_phy_tx_mode(tx_dma);

    Bb::r32(bb::Control) |= 0x10000000u;
    Bb::r32(bb::TxPower) &= ~3u;
    Lle::r32(lle::Control) = 2u;
}

void set_rx(Frame* frame, std::uint32_t timeout) noexcept
{
    if (frame == nullptr)
    {
        return;
    }

    Lle::r16(lle::WaitTime) = 4u;
    if ((Lle::r32(lle::Control) & 3u) != 0)
    {
        Lle::r32(lle::Control) |= 8u;
    }

    Bb::r32(bb::Control) = (Bb::r32(bb::Control) & 0xFFFCFFFFu) | 0x200000u;
    RfEnd::r32(rfend_reg::Control2) |= 0x33000000u;
    Lle::r32(lle::PhyControl) = 0x000F01D8u;

    configure_channel(kLinkFrequency);

    Bb::r32(bb::RxAccessAddress)   = kLinkAccessAddress;
    Bb::r32(bb::RxAccessAddressEx) = 0u;
    Bb::r32(bb::RxCrcInit)         = kLinkCrcInit;
    Bb::r32(bb::RxCrcPoly)         = (Bb::r32(bb::RxCrcPoly) & 0xFF000000u) | (kLinkCrcPoly & 0x00FFFFFFu);

    Lle::r32(lle::Timeout)         = timeout;
    Lle::r32(lle::Timer)           = 0;
    Lle::r32(lle::Irq)             = 0x200000u;

    set_phy_rx_mode(kMaxPacket);

    Lle::r32(lle::Control) = 1u;

    if (timeout != 0)
    {
        start_lle_timer(timeout, 0x200000u);
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
    if (((control >> 6) & 1u) != 0)
    {
        Bb::r32(bb::CrcStatus) = 0x60u;
        Lle::r32(lle::Irq)     = 0x200000u;
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
