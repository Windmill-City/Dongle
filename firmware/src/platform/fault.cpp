#include "ch572/compiler.hpp"
#include "ch572/platform/registers.hpp"

namespace ch572::sysreg
{

[[noreturn]] CH572_HIGH_CODE void software_reset() noexcept
{
    SafeAccess access;
    reset_watchdog_ctrl() |= kSoftwareReset;
    for (;;)
    {
    }
}

} // namespace ch572::sysreg

extern "C"
{
CH572_INTERRUPT CH572_HIGH_CODE void HardFault_Handler(void)
{
    ch572::sysreg::software_reset();
}

CH572_INTERRUPT CH572_HIGH_CODE void NMI_Handler(void)
{
    ch572::sysreg::software_reset();
}
}
