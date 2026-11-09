#include "ch572/platform/clock.hpp"

#include "ch572/config.hpp"
#include "ch572/platform/registers.hpp"

namespace ch572::clock {
namespace {

constexpr std::uint8_t kSysClockSelectMask = 0xC0;
constexpr std::uint8_t kSysClockSelectHse = 0xC0;
constexpr std::uint8_t kSysClockSelectPll = 0x40;
constexpr std::uint8_t kPll100MHz = kSysClockSelectPll | 6;
constexpr std::uint8_t kPowerPlanEnable = 0x40;
constexpr std::uint8_t kFlashPllConfig = 0x01;
constexpr std::uint8_t kFlashSckPll = 1u << 4;

} // namespace

CH572_HIGH_CODE void set_hse_capacitance(HseCapacitance capacitance) noexcept
{
    const auto value = static_cast<std::uint8_t>(capacitance);
    sysreg::SafeAccess access;
    const auto tune = sysreg::xt32m_tune();
    sysreg::xt32m_tune() = static_cast<std::uint8_t>((tune & 0x0Fu) | (value << 4));
}

CH572_HIGH_CODE void init_100mhz() noexcept
{
    if ((sysreg::hfck_power_ctrl() & sysreg::kClockXt32mOn) == 0)
    {
        const auto saved_tune = sysreg::xt32m_tune();
        {
            sysreg::SafeAccess access;
            sysreg::xt32m_tune() |= 0x03u;
            sysreg::hfck_power_ctrl() |= sysreg::kClockXt32mOn;
        }

        const auto saved_cfg = sysreg::clock_sys_cfg();
        {
            sysreg::SafeAccess access;
            sysreg::clock_sys_cfg() |= kSysClockSelectHse;
        }
        for (int i = 0; i < 9; ++i)
        {
            nop();
        }
        {
            sysreg::SafeAccess access;
            sysreg::clock_sys_cfg() = saved_cfg;
            sysreg::xt32m_tune() = saved_tune;
        }
    }

    {
        sysreg::SafeAccess access;
        sysreg::hfck_power_ctrl() |= sysreg::kClockPllOn;
        sysreg::flash_cfg() = kFlashPllConfig;
        sysreg::flash_sck() |= kFlashSckPll;
    }

    {
        sysreg::SafeAccess access;
        sysreg::sleep_power_ctrl() |= kPowerPlanEnable;
        sysreg::clock_sys_cfg() = kPll100MHz;
        // CH570Q SetSysClock(): 7,7 trims (check the CH572 SDK value separately).
        sysreg::misc_ctrl() |= (7u << 3) | (7u << 6);
    }
}

} // namespace ch572::clock
