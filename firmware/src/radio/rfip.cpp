#include "ch572/radio/rfip.hpp"

#include "ch572/config.hpp"
#include "ch572/platform/registers.hpp"
#include "ch572/radio/rfend.hpp"

namespace ch572::rf {


namespace {

using radio_reg::Bb;
using radio_reg::Lle;
using radio_reg::RfEnd;
namespace lle = radio_reg::lle;
namespace bb = radio_reg::bb;
namespace rfend_reg = radio_reg::rfend;
namespace rfend_block = ch572::rfend;

RoleConfig g_role_config{};
ProcessCallback g_lle_process = nullptr;
std::uint32_t g_wait_recv_time = 0;

CH572_HIGH_CODE void rf_basic_process(RoleState state) noexcept;

inline std::uint32_t read_u32(const void *address) noexcept
{
    return *static_cast<const std::uint32_t *>(address);
}

CH572_HIGH_CODE void set_channel(std::uint32_t frequency, std::uint32_t properties, std::uint8_t mode) noexcept
{
    if ((properties & 1u) == 0)
    {
        std::uint32_t code;
        if (frequency <= 0x24AE9Fu)
        {
            code = 0x25u;
        }
        else if (frequency <= 0x25048Fu)
        {
            code = ((frequency + 0xFFDB5930u) / 2000u - 1u) & 0x7Fu;
        }
        else if (frequency == 0x250490u)
        {
            code = 0x26u;
        }
        else if (frequency <= 0x25D77Fu)
        {
            code = ((frequency + 0xFFDB5930u) / 2000u - 2u) & 0x7Fu;
        }
        else
        {
            code = 0x27u;
        }

        const std::uint32_t value = Bb::r32(bb::Control);
        Bb::r32(bb::Control) = (value & 0xFFFFFF80u) | code;
    }

    if (mode == 1)
    {
        const std::uint32_t phy = properties & 0x30u;
        if (phy == 0x30u)
        {
            frequency -= (((properties >> 25) & 3u) == 0) ? 2000u : 1000u;
        }
        else if (phy == 0x10u)
        {
            frequency -= 2000u;
        }
        else
        {
            frequency -= 1000u;
        }
    }

    RfEnd::r32(rfend_reg::TxControl) |= 2u;
    RfEnd::r32(rfend_reg::ChannelDivider) =
        (RfEnd::r32(rfend_reg::ChannelDivider) & 0xFE0FFFFFu) |
        (((frequency / 0xFA00u) & 0x1Fu) << 20);
    RfEnd::r32(rfend_reg::ChannelDivider) =
        (RfEnd::r32(rfend_reg::ChannelDivider) & 0xFFFC0000u) |
        ((((frequency % 0xFA00u) * 0x400u) / 250u) & 0x3FFFu);
}

CH572_HIGH_CODE void start_lle_timer(std::uint32_t total, std::uint32_t bit) noexcept
{
    const std::uint32_t saved = Lle::r32(lle::Config2);
    Lle::r32(lle::Config2) = saved & ~bit;
    fence_i();
    Lle::r32(lle::Irq) = bit;
    Lle::r32(lle::Timer) = total;
    Lle::r32(lle::Config2) = saved;
}

CH572_HIGH_CODE void configure_channel(std::uint32_t frequency, std::uint32_t properties,
                                       std::uint8_t white_channel, std::uint8_t mode) noexcept
{
    if (frequency <= 0x231860u)
    {
        RfEnd::r32(rfend_reg::TxControl) &= ~2u;
        Bb::r32(bb::Control) =
            (Bb::r32(bb::Control) & 0xFFFFFF80u) | (frequency & 0x7Fu);
        return;
    }

    set_channel(frequency, properties, mode);

    if ((properties & 4u) != 0)
    {
        const std::uint32_t value = Bb::r32(bb::Control);
        const std::uint32_t white = ((value & 0x40u) | white_channel) & 0x7Fu;
        Bb::r32(bb::Control) = (value & 0xFFFFFF80u) | white;
    }
    if ((properties & 1u) != 0)
    {
        const std::uint32_t value = Bb::r32(bb::Control);
        Bb::r32(bb::Control) = (value & 0xFFFFFF80u) | ((value & 0x3Fu) | 0x40u);
    }
}

CH572_HIGH_CODE void set_phy_tx_mode(const TxParams &params) noexcept
{
    const std::uint32_t properties = params.properties;
    const std::uint32_t phy = properties & 0x30u;

    RfEnd::r32(rfend_reg::PhyConfig) =
        (RfEnd::r32(rfend_reg::PhyConfig) & ~0x700u) | (properties & 0x700u);

    if (phy == 0x10u)
    {
        Bb::r32(bb::Control) &= ~0x300u;
        const std::uint32_t dma_word = read_u32(reinterpret_cast<const void *>(params.tx_dma));
        Bb::r32(bb::Control3) &= 0xF9FFFFFFu;

        const std::uint32_t wait = Lle::r16(lle::WaitTime);

        const std::uint32_t length = (dma_word >> 8) & 0xFFu;
        const std::uint32_t total =
            (((length + 11u) << 2) + ((wait >> 1) + 44u)) << 1;
        start_lle_timer(total, 0x20000u);
        return;
    }

    if (phy == 0x30u)
    {
        Bb::r32(bb::Control) |= 0x300u;
        Bb::r16(bb::Mode2G4) = static_cast<std::uint16_t>((properties >> 16) | 0x2000u);

        const std::uint32_t control3 = Bb::r32(bb::Control3);
        std::uint8_t length;
        if ((control3 & 0x01000000u) != 0)
        {
            length = *reinterpret_cast<const std::uint8_t *>(params.tx_dma);
        }
        else
        {
            length = params.tx_len;
            Bb::r32(bb::Length) |= 0x100u;
            Bb::r8(bb::Length) = params.tx_len;
        }

        const std::uint32_t wait = Lle::r16(lle::WaitTime);

        std::uint32_t total;
        if (((control3 >> 25) & 3u) == 1u)
        {
            total = (((static_cast<std::uint32_t>(length) + 10u) << 3) +
                     ((wait >> 1) + 44u)) << 1;
        }
        else
        {
            total = (((static_cast<std::uint32_t>(length) + 11u) << 2) +
                     ((wait >> 1) + 44u)) << 1;
        }
        start_lle_timer(total, 0x20000u);
        return;
    }

    Bb::r32(bb::Control) = (Bb::r32(bb::Control) & ~0x300u) | 0x100u;
    const std::uint32_t dma_word = read_u32(reinterpret_cast<const void *>(params.tx_dma));
    Bb::r32(bb::Control3) &= 0xF9FFFFFFu;
    Bb::r32(bb::Control3) |= 0x20000000u;

    const std::uint32_t wait = Lle::r16(lle::WaitTime);

    const std::uint32_t length = (dma_word >> 8) & 0xFFu;
    const std::uint32_t total =
        ((((length + 10u) << 3) + ((wait >> 1) + 44u))) << 1;
    start_lle_timer(total, 0x20000u);
}

CH572_HIGH_CODE void set_phy_rx_mode(std::uint32_t properties, std::uint8_t rx_max_len) noexcept
{
    const std::uint32_t phy = properties & 0x30u;

    if (phy == 0x10u)
    {
        Bb::r32(bb::Control) &= ~0x300u;
        RfEnd::r32(rfend_reg::FilterControl) =
            (RfEnd::r32(rfend_reg::FilterControl) & ~0x1Fu) |
            (rfend_block::tune_filter_2m() & 0x1Fu);
        Bb::r32(bb::Control3) &= 0xF9FFFFFFu;
        Bb::r32(bb::Control1) = (Bb::r32(bb::Control1) & ~0x3Fu) | 13u;
        Bb::r8(bb::Control2High) = 127u;
        Bb::r32(bb::Control2) = (Bb::r32(bb::Control2) & ~0x3FFu) | 160u;
        g_wait_recv_time = ((static_cast<std::uint32_t>(rx_max_len) + 3u) << 3) + 60u;
        return;
    }

    if (phy == 0x30u)
    {
        Bb::r32(bb::Control) |= 0x300u;
        Bb::r16(bb::Mode2G4) =
            static_cast<std::uint16_t>((properties >> 16) | 0x2000u);

        const std::uint32_t control3 = Bb::r32(bb::Control3);
        if ((control3 & 0x01000000u) == 0)
        {
            Bb::r32(bb::Length) |= 0x100u;
            Bb::r8(bb::Length) = rx_max_len;
        }

        if (((control3 >> 25) & 3u) == 1u)
        {
            g_wait_recv_time = ((static_cast<std::uint32_t>(rx_max_len) + 3u) << 4) + 60u;
            Bb::r32(bb::Control1) = (Bb::r32(bb::Control1) & ~0x3Fu) | 11u;
            Bb::r8(bb::Control2High) = 120u;
            Bb::r32(bb::Control2) =
                (Bb::r32(bb::Control2) & ~0x3FFu) |
                ((((control3 >> 20) & 3u) == 0u) ? 124u : 156u);
            RfEnd::r32(rfend_reg::FilterControl) =
                (RfEnd::r32(rfend_reg::FilterControl) & ~0x1Fu) |
                (rfend_block::tune_filter() & 0x1Fu);
        }
        else
        {
            g_wait_recv_time = ((static_cast<std::uint32_t>(rx_max_len) + 3u) << 3) + 60u;
            Bb::r32(bb::Control1) = (Bb::r32(bb::Control1) & ~0x3Fu) | 13u;
            Bb::r8(bb::Control2High) = 127u;
            Bb::r32(bb::Control2) =
                (Bb::r32(bb::Control2) & ~0x3FFu) |
                ((((control3 >> 20) & 3u) == 0u) ? 128u : 160u);
            RfEnd::r32(rfend_reg::FilterControl) =
                (RfEnd::r32(rfend_reg::FilterControl) & ~0x1Fu) |
                (rfend_block::tune_filter_2m() & 0x1Fu);
        }
        return;
    }

    Bb::r32(bb::Control) = (Bb::r32(bb::Control) & ~0x300u) | 0x100u;
    RfEnd::r32(rfend_reg::FilterControl) =
        (RfEnd::r32(rfend_reg::FilterControl) & ~0x1Fu) |
        (rfend_block::tune_filter() & 0x1Fu);
    Bb::r32(bb::Control3) &= 0xF9FFFFFFu;
    Bb::r32(bb::Control3) |= 0x20000000u;
    Bb::r32(bb::Control1) = (Bb::r32(bb::Control1) & ~0x3Fu) | 11u;
    Bb::r8(bb::Control2High) = 120u;
    Bb::r32(bb::Control2) = (Bb::r32(bb::Control2) & ~0x3FFu) | 156u;
    g_wait_recv_time = ((static_cast<std::uint32_t>(rx_max_len) + 3u) << 4) + 60u;
}

CH572_HIGH_CODE void rf_basic_process(RoleState state) noexcept
{
    if ((state & kStateTimeout) != 0)
    {
        Lle::r32(lle::Timer) = 0;
        Lle::r32(lle::Irq) = 0x200000u;
        state &= ~(1u << 17);
        if (g_role_config.process_cb != nullptr &&
            (g_role_config.process_mask & kStateTxFinish) != 0)
        {
            g_role_config.process_cb(kStateTxFinish);
        }
    }

    if ((state & kStateRx) != 0)
    {
        Lle::r32(lle::Timer) = 0;
        Lle::r32(lle::Irq) = 0x200000u;
        if (read_crc() == 0)
        {
            if (g_role_config.process_cb != nullptr &&
                (g_role_config.process_mask & kStateRx) != 0)
            {
                g_role_config.process_cb(kStateRx);
            }
        }
        else
        {
            if (g_role_config.process_cb != nullptr &&
                (g_role_config.process_mask & kStateCrcError) != 0)
            {
                g_role_config.process_cb(kStateCrcError);
            }
        }
    }

    if ((state & (1u << 17)) != 0)
    {
        if (g_role_config.process_cb != nullptr &&
            (g_role_config.process_mask & kStateTimeout) != 0)
        {
            g_role_config.process_cb(kStateTimeout);
        }
    }
}

} // namespace

Status role_init(RoleConfig *config) noexcept
{
    if (config == nullptr)
    {
        return 1;
    }

    g_role_config = *config;
    g_lle_process = &rf_basic_process;
    sysreg::iwdg_feed();

    rfend_block::dev_init();
    sysreg::iwdg_feed();

    wakeup_reg_init();
    sysreg::iwdg_feed();

    calibration();
    sysreg::iwdg_feed();

    enable_irq(Irq::BleBb);
    enable_irq(Irq::BleLle);
    return 0;
}

void wakeup_reg_init() noexcept
{
    {
        sysreg::SafeAccess access;
        sysreg::sleep_clk_off1() &= ~0x80u;
        sysreg::sleep_clk_off2() &= ~0x13u;
    }
    Lle::r32(lle::Config) |= 0x1Eu;
    Lle::r32(lle::Config54) = 0;
    Lle::r32(lle::Config2) = 0x20003u;
    Bb::r32(bb::CrcStatus) = 0xFFFFu;
    Bb::r32(bb::RxConfig) = 64u;
    Bb::r32(0x38) = 0x20018u;
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

    Lle::r32(lle::Irq) = 0x20000u;
    Bb::r32(bb::Control) = (Bb::r32(bb::Control) & 0xFFFCFFFFu) | 0x10000u;
    RfEnd::r32(rfend_reg::Control2) &= 0xFFCDFFFFu;
    Lle::r32(lle::PhyControl) = 0x000F0080u;
}

std::uint8_t read_crc() noexcept
{
    return static_cast<std::uint8_t>((Bb::r32(bb::CrcStatus) >> 5) & 1u);
}

Status start_tx(TxParams *params) noexcept
{
    if (params == nullptr)
    {
        return 1;
    }

    Lle::r32(lle::Control) |= 8u;
    Bb::r32(bb::TxPower) =
        (Bb::r32(bb::TxPower) & 0x01FFFFFFu) |
        (static_cast<std::uint32_t>((static_cast<std::uint8_t>(params->tx_power) | 0x40u) & 0xFFu) << 25);
    Lle::r16(lle::WaitTime) = params->wait_time;
    Bb::r32(bb::TxPower) = (Bb::r32(bb::TxPower) & ~3u) | 1u;
    Bb::r32(bb::Control) = (Bb::r32(bb::Control) & 0xFFFCFFFFu) | 0x200000u;
    RfEnd::r32(rfend_reg::Control2) |= 0x33000000u;
    Lle::r32(lle::PhyControl) = 0x000F02D8u;

    configure_channel(params->frequency, params->properties, params->white_channel, 2);

    if (Lle::r16(lle::WaitTime) > 40u)
    {
        RfEnd::r32(rfend_reg::Control) &= ~0x100u;
        nop();
        nop();
        nop();
        nop();
        RfEnd::r32(rfend_reg::Control) |= 0x100u;
    }

    Bb::r32(bb::AccessAddress) = params->access_address;
    Bb::r32(bb::AccessAddressEx) = params->access_address_ex;
    Bb::r32(bb::CrcInit) = params->crc_init;
    Bb::r32(bb::CrcPoly) =
        (Bb::r32(bb::CrcPoly) & 0xFF000000u) | (params->crc_poly & 0x00FFFFFFu);
    Lle::r32(lle::TxDma) = params->tx_dma;

    set_phy_tx_mode(*params);

    Bb::r32(bb::Control) |= 0x10000000u;
    Bb::r32(bb::TxPower) &= ~3u;
    Lle::r32(lle::Control) = 2u;
    return 0;
}

Status set_rx(RxParams *params) noexcept
{
    if (params == nullptr)
    {
        return 1;
    }
    if (params->rx_dma == 0)
    {
        return 2;
    }

    Lle::r16(lle::WaitTime) = 4u;
    if ((Lle::r32(lle::Control) & 3u) != 0)
    {
        Lle::r32(lle::Control) |= 8u;
    }

    Bb::r32(bb::Control) = (Bb::r32(bb::Control) & 0xFFFCFFFFu) | 0x200000u;
    RfEnd::r32(rfend_reg::Control2) |= 0x33000000u;
    Lle::r32(lle::PhyControl) = 0x000F01D8u;

    configure_channel(params->frequency, params->properties, params->white_channel, 1);

    Bb::r32(bb::RxAccessAddress) = params->access_address;
    Bb::r32(bb::RxAccessAddressEx) = params->access_address_ex;
    Bb::r32(bb::RxCrcInit) = params->crc_init;
    Bb::r32(bb::RxCrcPoly) =
        (Bb::r32(bb::RxCrcPoly) & 0xFF000000u) | (params->crc_poly & 0x00FFFFFFu);

    Lle::r32(lle::Timeout) = params->time_out;
    Lle::r32(lle::Timer) = 0;
    Lle::r32(lle::Irq) = 0x200000u;

    set_phy_rx_mode(params->properties, params->rx_max_len);

    Lle::r32(lle::Control) = 1u;

    if (params->time_out != 0)
    {
        start_lle_timer(params->time_out, 0x200000u);
    }

    return 0;
}

Status stop() noexcept
{
    Lle::r32(lle::PhyControl) &= ~0x10u;
    if ((Lle::r32(lle::Control) & 3u) != 0)
    {
        Lle::r32(lle::Control) |= 8u;
        nop();
        nop();
        Lle::r32(lle::PhyControl) =
            (Lle::r32(lle::PhyControl) & ~0x700u) | 0x500u;
    }
    return 0;
}

Status shutdown() noexcept
{
    stop();
    Bb::r32(bb::Control) =
        (Bb::r32(bb::Control) & 0xFFFCFFFFu) | 0x10000u;
    RfEnd::r32(rfend_reg::Control2) &= 0xFFCDFFFFu;
    Lle::r32(lle::PhyControl) = 0x000F0080u;
    return 0;
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
    if (g_lle_process != nullptr)
    {
        g_lle_process(status & enable);
    }
}

void bb_irq_handler() noexcept
{
    const std::uint32_t control = Bb::r32(bb::Control);
    if (((control >> 6) & 1u) != 0)
    {
        Bb::r32(bb::CrcStatus) = 0x60u;
        Lle::r32(lle::Irq) = 0x200000u;
        Lle::r32(lle::Timer) = g_wait_recv_time;
    }
}

} // namespace ch572::rf

extern "C" {

CH572_INTERRUPT CH572_HIGH_CODE void LLE_IRQHandler(void)
{
    ch572::rf::lle_irq_handler();
}

CH572_INTERRUPT CH572_HIGH_CODE void BB_IRQHandler(void)
{
    ch572::rf::bb_irq_handler();
}

}
