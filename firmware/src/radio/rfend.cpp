#include "ch572/radio/rfend.hpp"

#include "ch572/compiler.hpp"
#include "ch572/platform/registers.hpp"

namespace ch572::rfend
{
namespace
{

using radio_reg::Lle;
using radio_reg::RfEnd;
namespace off                               = radio_reg::rfend;
namespace lle                               = radio_reg::lle;

// Measured calibration value for the fixed 2440 MHz channel.
std::uint8_t            g_nco2440           = 0;

constexpr std::uint32_t kCalibrationTimeout = 10000000u;

} // namespace

void dev_init() noexcept
{
    RfEnd::r32(off::TxBias)        = 1152;
    RfEnd::r32(off::Power)         = (RfEnd::r32(off::Power) & 0xFFFF0FFFu) | 0x00008000u;
    RfEnd::r32(off::TxControl)     = (RfEnd::r32(off::TxControl) & 0xFFFFCFFFu) | 0x00001000u;
    RfEnd::r32(off::TxControl)     = (RfEnd::r32(off::TxControl) & 0xFFFCFFFFu) | 0x00010000u;
    RfEnd::r32(off::Power)         = (RfEnd::r32(off::Power) & 0xF8FFFFFFu) | 0x20000000u;
    RfEnd::r32(off::Power)         = (RfEnd::r32(off::Power) & 0x1FFFFFFFu) | 0x00040000u;
    RfEnd::r32(off::TxControl2)    = RfEnd::r32(off::TxControl2) & 0xFFF9FFFFu;
    RfEnd::r32(off::TxControl2)    = (RfEnd::r32(off::TxControl2) & 0x8FFFFFFFu) | 0x00040000u;
    RfEnd::r32(off::TuneControl)   = RfEnd::r32(off::TuneControl) & 0xFFF8FFFFu;
    RfEnd::r32(off::FilterControl) = (RfEnd::r32(off::FilterControl) & ~0x700u) | 0x300u;
    RfEnd::r32(off::RxControl) |= 0x00070000u;
    RfEnd::r32(off::RxControl) |= 0x07000000u;
    RfEnd::r32(off::TxBias) |= 0x00000800u;
    for (int i = 0; i < 100; ++i)
    {
        nop();
    }
}

namespace
{

void rx_filter() noexcept
{
    RfEnd::r32(off::FilterControl) &= 0xFFFEFFFFu;
    RfEnd::r32(off::Control2) |= 0x00200000u;
    RfEnd::r32(off::Control3) |= 0x10u;
    RfEnd::r32(off::Control3) &= ~0x10u;
    nop();
    nop();
    nop();
    nop();
    RfEnd::r32(off::Control3) |= 0x10u;
    RfEnd::r32(off::Control) |= 0x00010000u;

    Lle::r32(lle::Timer) = 100;
    for (volatile std::uint32_t i = 0; i < 1000000u; ++i)
    {
        if ((i & 0x3FFFu) == 0u)
        {
            sysreg::iwdg_feed();
        }
        nop();
    }
    for (std::uint32_t i = 0; i < kCalibrationTimeout; ++i)
    {
        if ((i & 0x3FFFu) == 0u)
        {
            sysreg::iwdg_feed();
        }
        if ((RfEnd::r32(off::FilterResult) & 0x100u) != 0)
        {
            break;
        }
    }

    nop();
    nop();
    nop();
    nop();

    const std::uint8_t tune = static_cast<std::uint8_t>(RfEnd::r32(off::FilterResult) & 0x1Fu);
    RfEnd::r32(off::FilterControl) |= 0x00010000u;
    RfEnd::r32(off::FilterControl) = (RfEnd::r32(off::FilterControl) & ~0x1Fu) | tune;
    RfEnd::r32(off::Control2) &= 0xFFDFFFFFu;

    if (tune > 3)
    {
        detail::g_tune_filter_2m = static_cast<std::uint8_t>(tune - 3);
    }
}

void rx_adc() noexcept
{
    RfEnd::r32(off::AdcControl) &= 0xFFFEFFFFu;
    RfEnd::r32(off::Control2) |= 0x00010000u;
    RfEnd::r32(off::Control3) &= ~0x100u;
    RfEnd::r32(off::Control) &= 0xFFFEFFFFu;
    RfEnd::r32(off::Control3) |= 0x100u;
    RfEnd::r32(off::Control) |= 0x00010000u;
}

void wait_tune(std::uint16_t cycles) noexcept
{
    RfEnd::r32(off::Control) &= ~1u;
    RfEnd::r32(off::DacRef) =
        (RfEnd::r32(off::DacRef) & 0xFFFE00FFu) | (static_cast<std::uint32_t>(cycles & 0x1FFu) << 8);
    RfEnd::r32(off::Control) |= 1u;
    Lle::r32(lle::Timer) = 0x1F40u;

    for (std::uint32_t i = 0; i < kCalibrationTimeout; ++i)
    {
        if ((i & 0x3FFFu) == 0u)
        {
            sysreg::iwdg_feed();
        }
        if (Lle::r32(lle::Timer) == 0)
        {
            break;
        }
        const std::uint32_t value = RfEnd::r32(off::TuneResult);
        if ((value & 0x06000000u) == 0x06000000u)
        {
            break;
        }
    }
}

void tx_ftune() noexcept
{
    RfEnd::r32(off::Control) |= 0x100u;
}

void tx_ctune() noexcept
{
    RfEnd::r32(off::Control) &= ~0x100u;
    RfEnd::r32(off::TxBias) &= ~0x1000u;
    RfEnd::r32(off::TxControl) &= ~0x10u;
    RfEnd::r32(off::Control2) |= 0x00020000u;
    RfEnd::r32(off::Control) |= 0x10u;

    wait_tune(211);
    g_nco2440 = static_cast<std::uint8_t>(RfEnd::r32(off::TuneResult) & 0x3Fu);

    RfEnd::r32(off::Control) &= ~0x10u;
    RfEnd::r32(off::Control) &= ~1u;
    RfEnd::r32(off::TxBias) |= 0x1000u;
    RfEnd::r32(off::TxControl) |= 0x10u;
    RfEnd::r32(off::DacRef) = (RfEnd::r32(off::DacRef) & ~0x3Fu) | (g_nco2440 & 0x3Fu);
    RfEnd::r32(off::DacRef) = (RfEnd::r32(off::DacRef) & 0x80FFFFFFu) | (static_cast<std::uint32_t>(g_nco2440) << 24);
}

} // namespace

void calibration() noexcept
{
    tx_ctune();
    tx_ftune();
    rx_filter();
    rx_adc();
}

} // namespace ch572::rfend
