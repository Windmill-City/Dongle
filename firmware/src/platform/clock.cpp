#include "ch572/platform/clock.hpp"

#include "ch572/compiler.hpp"
#include "ch572/platform/registers.hpp"

namespace ch572::clock
{

CH572_HIGH_CODE void init() noexcept
{
    {
        sysreg::SafeAccess access;
        sysreg::xt32m_tune() = static_cast<std::uint8_t>((sysreg::xt32m_tune() & 0x0Fu) | kHseCapacitance);
    }

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
            sysreg::xt32m_tune()    = saved_tune;
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
        sysreg::clock_sys_cfg() = kPll;
        // 7,7 trims from the CH570Q SetSysClock().
        sysreg::misc_ctrl() |= (7u << 3) | (7u << 6);
    }

    // Start the free-running SysTick used by the delay/timeout helpers. It
    // runs from HCLK/8, so it must start after the PLL is selected.
    systick::now()  = 0;
    systick::ctrl() = systick::kCtrlEnable;
}

} // namespace ch572::clock
