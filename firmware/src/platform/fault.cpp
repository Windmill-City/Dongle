#include "ch572/config.hpp"
#include "ch572/platform/registers.hpp"

namespace {

CH572_HIGH_CODE void fault_reset() noexcept
{
    ch572::sysreg::SafeAccess access;
    ch572::sysreg::reset_watchdog_ctrl() |= ch572::sysreg::kSoftwareReset;
    for (;;)
    {
    }
}

} // namespace

extern "C" {

CH572_INTERRUPT CH572_HIGH_CODE void HardFault_Handler(void)
{
    fault_reset();
}

CH572_INTERRUPT CH572_HIGH_CODE void NMI_Handler(void)
{
    fault_reset();
}

}
