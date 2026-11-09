#include "ch572/radio/rfend.hpp"

#include "ch572/platform/registers.hpp"


namespace ch572::rfend {
namespace {

using radio_reg::Bb;
using radio_reg::Lle;
using radio_reg::RfEnd;
namespace off = radio_reg::rfend;
namespace lle = radio_reg::lle;
namespace bb = radio_reg::bb;

// Measured calibration values, filled by tx_ctune() and restorable with
// tx_ctune_set().
std::uint8_t g_nco2401 = 0;
std::uint8_t g_nga2401 = 0;
std::uint8_t g_nco2440 = 0;
std::uint8_t g_nga2440 = 0;
std::uint8_t g_nco2480 = 0;
std::uint8_t g_nga2480 = 0;
std::uint16_t g_ftune_flag = 0;

std::uint8_t g_tune_filter = 0;
std::uint8_t g_tune_filter_2m = 0;
std::uint32_t g_wait_recv_time = 0;

constexpr std::uint32_t kCalibrationTimeout = 10000000u;

// Match the RISC-V DIV instruction's defined behaviour for a zero divisor.
constexpr std::int32_t signed_div(std::int32_t lhs, std::int32_t rhs) noexcept
{
    return rhs == 0 ? -1 : lhs / rhs;
}

std::uint32_t tx_ctune_call(std::int32_t index) noexcept
{
    const std::int32_t delta =
        static_cast<std::int32_t>(g_nco2480) - static_cast<std::int32_t>(g_nco2440);
    std::int32_t acc = (32 - index) * delta;
    std::uint32_t result = 0;

    for (int i = 0; i < 8; ++i)
    {
        result <<= 4;
        result |= static_cast<std::uint32_t>(signed_div(acc, 39));
        acc += delta;
    }
    return result;
}

std::uint32_t tx_ctune_callx(std::int32_t index) noexcept
{
    const std::int32_t delta =
        static_cast<std::int32_t>(g_nco2480) - static_cast<std::int32_t>(g_nco2440);
    std::int32_t acc = -delta * (index + 7);
    std::uint32_t result = 0;

    for (int i = 0; i < 8; ++i)
    {
        result <<= 4;
        result |= static_cast<std::uint32_t>(signed_div(acc, 39));
        acc += delta;
    }
    return result;
}

} // namespace

void dev_init() noexcept
{
    RfEnd::r32(off::TxBias) = 1152;
    RfEnd::r32(off::Power) = (RfEnd::r32(off::Power) & 0xFFFF0FFFu) | 0x00008000u;
    RfEnd::r32(off::TxControl) =
        (RfEnd::r32(off::TxControl) & 0xFFFFCFFFu) | 0x00001000u;
    RfEnd::r32(off::TxControl) =
        (RfEnd::r32(off::TxControl) & 0xFFFCFFFFu) | 0x00010000u;
    RfEnd::r32(off::Power) = (RfEnd::r32(off::Power) & 0xF8FFFFFFu) | 0x20000000u;
    RfEnd::r32(off::Power) = (RfEnd::r32(off::Power) & 0x1FFFFFFFu) | 0x00040000u;
    RfEnd::r32(off::TxControl2) = RfEnd::r32(off::TxControl2) & 0xFFF9FFFFu;
    RfEnd::r32(off::TxControl2) =
        (RfEnd::r32(off::TxControl2) & 0x8FFFFFFFu) | 0x00040000u;
    RfEnd::r32(off::TuneControl) = RfEnd::r32(off::TuneControl) & 0xFFF8FFFFu;
    RfEnd::r32(off::FilterControl) =
        (RfEnd::r32(off::FilterControl) & ~0x700u) | 0x300u;
    RfEnd::r32(off::RxControl) |= 0x00070000u;
    RfEnd::r32(off::RxControl) |= 0x07000000u;
    RfEnd::r32(off::TxBias) |= 0x00000800u;
    for (int i = 0; i < 100; ++i)
    {
        nop();
    }
}

void set_dacref_tune(std::uint8_t value) noexcept
{
    RfEnd::r32(off::DacRef) =
        (RfEnd::r32(off::DacRef) & 0xFF07FFFFu) |
        (static_cast<std::uint32_t>(value & 0x1Fu) << 19);
}

std::uint8_t get_dacref_tune() noexcept
{
    return static_cast<std::uint8_t>((RfEnd::r32(off::DacRef) >> 19) & 0x1Fu);
}

void set_ex_power() noexcept
{
    Bb::r32(bb::RxConfig) |= 0xF0Fu;
}

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
    g_tune_filter = tune;
    RfEnd::r32(off::FilterControl) |= 0x00010000u;
    RfEnd::r32(off::FilterControl) =
        (RfEnd::r32(off::FilterControl) & ~0x1Fu) | tune;
    RfEnd::r32(off::Control2) &= 0xFFDFFFFFu;

    if (tune > 3)
    {
        g_tune_filter_2m = static_cast<std::uint8_t>(tune - 3);
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
        (RfEnd::r32(off::DacRef) & 0xFFFE00FFu) |
        (static_cast<std::uint32_t>(cycles & 0x1FFu) << 8);
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

void tx_ctune_set(const std::uint8_t *values) noexcept
{
    if (values == nullptr || values[0] != 6)
    {
        return;
    }

    g_nco2401 = values[1];
    g_nga2401 = values[2];
    g_nco2440 = values[3];
    g_nga2440 = values[4];
    g_nco2480 = values[5];
    g_nga2480 = values[6];

    g_ftune_flag = (g_nco2401 != 0 && g_nga2401 != 0) ? 0x1234u : 0u;
}

void tx_ctune_get(std::uint8_t *values) noexcept
{
    if (values == nullptr)
    {
        return;
    }

    values[0] = 6;
    values[1] = g_nco2401;
    values[2] = g_nga2401;
    values[3] = g_nco2440;
    values[4] = g_nga2440;
    values[5] = g_nco2480;
    values[6] = g_nga2480;
}

void tx_ctune() noexcept
{
    RfEnd::r32(off::Control) &= ~0x100u;
    RfEnd::r32(off::TxBias) &= ~0x1000u;
    RfEnd::r32(off::TxControl) &= ~0x10u;
    RfEnd::r32(off::Control2) |= 0x00020000u;
    RfEnd::r32(off::Control) |= 0x10u;

    if (g_ftune_flag != 0x1234u)
    {
        wait_tune(191);
        g_nco2401 = static_cast<std::uint8_t>(RfEnd::r32(off::TuneResult) & 0x3Fu);
        g_nga2401 = static_cast<std::uint8_t>((RfEnd::r32(off::GainResult) >> 10) & 0x7Fu);

        wait_tune(231);
        g_nco2480 = static_cast<std::uint8_t>(RfEnd::r32(off::TuneResult) & 0x3Fu);
        g_nga2480 = static_cast<std::uint8_t>((RfEnd::r32(off::GainResult) >> 10) & 0x7Fu);

        wait_tune(211);
        g_nco2440 = static_cast<std::uint8_t>(RfEnd::r32(off::TuneResult) & 0x3Fu);
        g_nga2440 = static_cast<std::uint8_t>((RfEnd::r32(off::GainResult) >> 10) & 0x7Fu);
    }
    else
    {
        g_ftune_flag = 0;
    }

    RfEnd::r32(off::TxTable0 + 0) = tx_ctune_call(0);
    RfEnd::r32(off::TxTable0 + 4) = tx_ctune_call(8);
    RfEnd::r32(off::TxTable0 + 8) = tx_ctune_call(16);
    RfEnd::r32(off::TxTable0 + 12) = tx_ctune_call(24);
    RfEnd::r32(off::TxTable0 + 16) = tx_ctune_call(32);
    RfEnd::r32(off::TxTable0 + 20) = tx_ctune_callx(1);
    RfEnd::r32(off::TxTable0 + 24) = tx_ctune_callx(9);
    RfEnd::r32(off::TxTable0 + 28) = tx_ctune_callx(17);
    RfEnd::r32(off::TxTable0 + 32) = tx_ctune_callx(25);
    RfEnd::r32(off::TxTable0 + 36) = tx_ctune_callx(33);

    const std::int32_t co_delta =
        static_cast<std::int32_t>(g_nco2440) - static_cast<std::int32_t>(g_nco2480);
    const std::int32_t ga_delta =
        static_cast<std::int32_t>(g_nga2440) - static_cast<std::int32_t>(g_nga2480);

    std::int32_t first = signed_div(co_delta * 42, 40);
    const std::int32_t second = signed_div(co_delta * 41, 40);
    first = (first << 4) | second;
    RfEnd::r32(off::TxTable0 + 40) = static_cast<std::uint32_t>(first);

    std::int32_t ga_step = ga_delta * 10;
    const std::int32_t ga_inc = -ga_delta;

    std::uint32_t table = RfEnd::r32(off::TxTable0 + 40);
    for (int shift = 8; shift != 32; shift += 4)
    {
        const std::int32_t nibble = signed_div(ga_step, co_delta);
        ga_step += ga_inc;
        table |= static_cast<std::uint32_t>((nibble | 8) << shift);
        RfEnd::r32(off::TxTable0 + 40) = table;
    }

    RfEnd::r32(off::TxTable0 + 44) = 0;
    std::int32_t ga_quad = ga_delta * 4;
    for (int shift = 0; shift != 16; shift += 4)
    {
        const std::int32_t nibble = signed_div(ga_quad, co_delta);
        ga_quad += ga_inc;
        RfEnd::r32(off::TxTable0 + 44) |= static_cast<std::uint32_t>((nibble | 8) << shift);
    }

    const std::int32_t ga2401_delta =
        static_cast<std::int32_t>(g_nga2401) - static_cast<std::int32_t>(g_nga2440);
    const std::int32_t co2401_delta =
        static_cast<std::int32_t>(g_nco2401) - static_cast<std::int32_t>(g_nco2440);

    std::int32_t acc = 0;
    for (int shift = 16; shift != 32; shift += 4)
    {
        const std::int32_t nibble = signed_div(acc, co2401_delta);
        acc += ga2401_delta;
        RfEnd::r32(off::TxTable0 + 44) |=
            static_cast<std::uint32_t>(nibble << shift);
    }

    std::int32_t acc2 = ga2401_delta << 2;
    for (int shift = 0; shift != 28; shift += 4)
    {
        const std::int32_t nibble = signed_div(acc2, co2401_delta);
        acc2 += ga2401_delta;
        RfEnd::r32(off::TxTable0 + 48) |=
            static_cast<std::uint32_t>(nibble << shift);
    }

    RfEnd::r32(off::Control) &= ~0x10u;
    RfEnd::r32(off::Control) &= ~1u;
    RfEnd::r32(off::TxBias) |= 0x1000u;
    RfEnd::r32(off::TxControl) |= 0x10u;
    RfEnd::r32(off::DacRef) =
        (RfEnd::r32(off::DacRef) & ~0x3Fu) | (g_nco2440 & 0x3Fu);
    RfEnd::r32(off::DacRef) =
        (RfEnd::r32(off::DacRef) & 0x80FFFFFFu) |
        (static_cast<std::uint32_t>(g_nco2480 & 0x7Fu) << 24);
}

void calibration() noexcept
{
    tx_ctune();
    tx_ftune();
    rx_filter();
    rx_adc();
}

std::uint8_t tune_filter() noexcept
{
    return g_tune_filter;
}

std::uint8_t tune_filter_2m() noexcept
{
    return g_tune_filter_2m;
}

std::uint32_t wait_recv_time() noexcept
{
    return g_wait_recv_time;
}

void set_wait_recv_time(std::uint32_t value) noexcept
{
    g_wait_recv_time = value;
}

} // namespace ch572::rfend
