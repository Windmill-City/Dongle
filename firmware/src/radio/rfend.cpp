#include "ch572/radio/rfend.hpp"

#include "ch572/platform/registers.hpp"

namespace ch572::rfend
{
namespace
{

using radio_reg::Lle;
using radio_reg::RfEnd;
namespace off                               = radio_reg::rfend;
namespace lle                               = radio_reg::lle;

// The LLE countdown timer runs at 2 MHz on CH570/CH572.
constexpr std::uint32_t kTimerTicksPerUs    = 2;
constexpr std::uint32_t kFilterTimeoutTicks = 100; // 50 us at 2 MHz
constexpr std::uint32_t kTuneTimeoutTicks   = 4000u * kTimerTicksPerUs;

// Vendor tuning points: 2401, 2480 and 2440 MHz.
constexpr std::uint16_t kTuneCycle2401      = 191;
constexpr std::uint16_t kTuneCycle2480      = 231;
constexpr std::uint16_t kTuneCycle2440      = 211;

// The vendor uses a short nop loop after enabling the TX bias. The LLE timer
// is already clocked at reset, so use it to express the same settling delay.
constexpr std::uint32_t kInitSettleTicks    = 6; // ~3 us at 2 MHz

struct TxTunePoint
{
    std::uint8_t nco;
    std::uint8_t nga;
};

// RISC-V "div" returns -1 on a zero divisor; keep the same behaviour here.
std::int32_t divide(std::int32_t numerator, std::int32_t denominator) noexcept
{
    return denominator == 0 ? -1 : numerator / denominator;
}

} // namespace

void dev_init() noexcept
{
    RfEnd::r32(off::TxBias)        = 1152;
    RfEnd::r32(off::Power)         = (RfEnd::r32(off::Power) & 0xF8FFFFFFu) | 0x20000000u;
    RfEnd::r32(off::Power)         = (RfEnd::r32(off::Power) & 0x1FFFFFFFu) | 0x00040000u;
    RfEnd::r32(off::TxControl2)    = RfEnd::r32(off::TxControl2) & 0xFFF9FFFFu;
    RfEnd::r32(off::TxControl2)    = (RfEnd::r32(off::TxControl2) & 0x8FFFFFFFu) | 0x00040000u;
    RfEnd::r32(off::TuneControl)   = RfEnd::r32(off::TuneControl) & 0xFFF8FFFFu;
    RfEnd::r32(off::FilterControl) = (RfEnd::r32(off::FilterControl) & ~0x700u) | 0x300u;
    RfEnd::r32(off::RxControl) |= 0x00070000u;
    RfEnd::r32(off::RxControl) |= 0x07000000u;
    RfEnd::r32(off::TxBias) |= 0x00000800u;

    Lle::r32(lle::Timer) = kInitSettleTicks;
    while (Lle::r32(lle::Timer) != 0)
    {
    }
}

namespace
{

void rx_filter() noexcept
{
    RfEnd::r32(off::FilterControl) &= ~off::kFilterEnable;
    RfEnd::r32(off::Control2) |= off::kControl2RxFilter;
    RfEnd::r32(off::Control3) |= off::kControl3RxFilterStart;
    RfEnd::r32(off::Control3) &= ~off::kControl3RxFilterStart;
    nop();
    nop();
    nop();
    nop();
    RfEnd::r32(off::Control3) |= off::kControl3RxFilterStart;
    RfEnd::r32(off::Control1) |= off::kControl1RxFilter;

    Lle::r32(lle::Timer) = kFilterTimeoutTicks;
    while (Lle::r32(lle::Timer) != 0 && (RfEnd::r32(off::FilterResult) & off::kFilterResultDone) == 0)
    {
    }

    nop();
    nop();
    nop();
    nop();

    const std::uint8_t tune = static_cast<std::uint8_t>(RfEnd::r32(off::FilterResult) & off::kFilterResultMask);
    RfEnd::r32(off::FilterControl) |= off::kFilterEnable;
    RfEnd::r32(off::FilterControl) = (RfEnd::r32(off::FilterControl) & ~off::kFilterTuneMask) | tune;
    RfEnd::r32(off::Control2) &= ~off::kControl2RxFilter;

    if (tune > 3)
    {
        detail::g_tune_filter_2m = static_cast<std::uint8_t>(tune - 3);
    }
}

void rx_adc() noexcept
{
    RfEnd::r32(off::AdcControl) &= ~off::kAdcControlEnable;
    RfEnd::r32(off::Control2) |= off::kControl2RxAdc;
    RfEnd::r32(off::Control3) &= ~off::kControl3RxAdcStart;
    RfEnd::r32(off::Control1) &= ~off::kControl1RxAdc;
    RfEnd::r32(off::Control3) |= off::kControl3RxAdcStart;
    RfEnd::r32(off::Control1) |= off::kControl1RxAdc;
}

void wait_tune(std::uint16_t cycles) noexcept
{
    RfEnd::r32(off::Control1) &= ~off::kControl1TuneStart;
    RfEnd::r32(off::DacRef) = (RfEnd::r32(off::DacRef) & ~(off::kDacRefTuneMask << off::kDacRefTuneShift))
                            | ((static_cast<std::uint32_t>(cycles) & off::kDacRefTuneMask) << off::kDacRefTuneShift);
    RfEnd::r32(off::Control1) |= off::kControl1TuneStart;
    Lle::r32(lle::Timer) = kTuneTimeoutTicks;

    while (Lle::r32(lle::Timer) != 0)
    {
        if ((RfEnd::r32(off::TuneResult) & off::kTuneActive) == 0)
        {
            continue;
        }
        if ((RfEnd::r32(off::TuneResult) & off::kTuneDone) != 0)
        {
            break;
        }
    }
}

TxTunePoint measure_tx_tune(std::uint16_t cycles) noexcept
{
    wait_tune(cycles);
    return TxTunePoint{
        static_cast<std::uint8_t>(RfEnd::r32(off::TuneResult) & off::kTuneResultCoMask),
        static_cast<std::uint8_t>((RfEnd::r32(off::TuneResultGa) >> off::kTuneResultGaShift) & off::kDacRefGaMask),
    };
}

std::uint32_t pack_co_low(std::uint8_t start, std::uint8_t nco2401, std::uint8_t nco2440) noexcept
{
    const std::int32_t delta  = static_cast<std::int32_t>(nco2401) - static_cast<std::int32_t>(nco2440);
    std::int32_t       value  = static_cast<std::int32_t>(32u - start) * delta;
    std::uint32_t      packed = 0;

    for (int i = 0; i < 8; ++i)
    {
        packed = (packed << 4) | static_cast<std::uint32_t>(divide(value, 39));
        value += delta;
    }
    return packed;
}

std::uint32_t pack_co_high(std::uint8_t start, std::uint8_t nco2440, std::uint8_t nco2480) noexcept
{
    const std::int32_t delta  = static_cast<std::int32_t>(nco2480) - static_cast<std::int32_t>(nco2440);
    const std::int32_t step   = static_cast<std::int32_t>(nco2440) - static_cast<std::int32_t>(nco2480);
    std::int32_t       value  = step * static_cast<std::int32_t>(start + 7u);
    std::uint32_t      packed = 0;

    for (int i = 0; i < 8; ++i)
    {
        packed = (packed << 4) | static_cast<std::uint32_t>(divide(value, 39));
        value += delta;
    }
    return packed;
}

void tx_ftune() noexcept
{
    RfEnd::r32(off::Control1) |= off::kControl1TxFine;
}

void tx_ctune() noexcept
{
    RfEnd::r32(off::Control1) &= ~off::kControl1TxFine;
    RfEnd::r32(off::TxBias) &= ~off::kTxBiasCtuneEnable;
    RfEnd::r32(off::TxControl1) &= ~off::kTxControl1TxEnable;
    RfEnd::r32(off::Control2) |= off::kControl2TxCtune;
    RfEnd::r32(off::Control1) |= off::kControl1TxCoarse;

    const TxTunePoint p2401 = measure_tx_tune(kTuneCycle2401);
    const TxTunePoint p2480 = measure_tx_tune(kTuneCycle2480);
    const TxTunePoint p2440 = measure_tx_tune(kTuneCycle2440);

    for (int i = 0; i < 5; ++i)
    {
        const auto index                        = static_cast<std::size_t>(i) * sizeof(std::uint32_t);
        RfEnd::r32(off::TuneTableCoLow + index) = pack_co_low(static_cast<std::uint8_t>(i * 8), p2401.nco, p2440.nco);
    }

    for (int i = 0; i < 5; ++i)
    {
        const auto index = static_cast<std::size_t>(i) * sizeof(std::uint32_t);
        RfEnd::r32(off::TuneTableCoHigh + index) =
            pack_co_high(static_cast<std::uint8_t>(1 + i * 8), p2440.nco, p2480.nco);
    }

    const std::int32_t dlo    = static_cast<std::int32_t>(p2401.nco) - p2440.nco;
    const std::int32_t dhi    = static_cast<std::int32_t>(p2440.nco) - p2480.nco;
    const std::int32_t dga_lo = static_cast<std::int32_t>(p2401.nga) - p2440.nga;
    const std::int32_t dga_hi = static_cast<std::int32_t>(p2440.nga) - p2480.nga;

    std::uint32_t      ga0 =
        (static_cast<std::uint32_t>(divide(dhi * 42, 40)) << 4) | static_cast<std::uint32_t>(divide(dhi * 41, 40));

    std::int32_t value = dga_hi * 40;
    for (int i = 0; i < 6; ++i)
    {
        ga0 |= static_cast<std::uint32_t>(divide(value, dhi) | 8) << (8 + i * 4);
        value -= dga_hi;
    }

    std::uint32_t ga1 = 0;
    value             = dga_hi * 4;
    for (int i = 0; i < 4; ++i)
    {
        ga1 |= static_cast<std::uint32_t>(divide(value, dhi) | 8) << (i * 4);
        value -= dga_hi;
    }

    value = 0;
    for (int i = 0; i < 4; ++i)
    {
        ga1 |= static_cast<std::uint32_t>(divide(value, dlo)) << (16 + i * 4);
        value += dga_lo;
    }

    std::uint32_t ga2 = RfEnd::r32(off::TuneTableGa + 2 * sizeof(std::uint32_t));
    value             = dga_lo * 4;
    for (int i = 0; i < 7; ++i)
    {
        ga2 |= static_cast<std::uint32_t>(divide(value, dlo)) << (i * 4);
        value += dga_lo;
    }

    RfEnd::r32(off::TuneTableGa)                             = ga0;
    RfEnd::r32(off::TuneTableGa + sizeof(std::uint32_t))     = ga1;
    RfEnd::r32(off::TuneTableGa + 2 * sizeof(std::uint32_t)) = ga2;

    RfEnd::r32(off::Control1) &= ~off::kControl1TxCoarse;
    RfEnd::r32(off::Control1) &= ~off::kControl1TuneStart;
    RfEnd::r32(off::TxBias) |= off::kTxBiasCtuneEnable;
    RfEnd::r32(off::TxControl1) |= off::kTxControl1TxEnable;
    RfEnd::r32(off::DacRef) = (RfEnd::r32(off::DacRef) & ~off::kDacRefCoMask) | static_cast<std::uint32_t>(p2440.nco);
    RfEnd::r32(off::DacRef) =
        (RfEnd::r32(off::DacRef) & 0x80FFFFFFu) | (static_cast<std::uint32_t>(p2440.nga) << off::kDacRefGaShift);
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
