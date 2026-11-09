#include "ch572/boot/iap_flag.hpp"

#include <cstdint>

#include "ch572/platform/registers.hpp"

namespace ch572::iap {
namespace {

// jump() requests the next boot by issuing a software reset, so the
// request has to outlive the reset. Keep it in the .noinit block at the top of
// SRAM, which the startup code does not clear. A watchdog reset must keep it
// too: if the application dies after requesting IAP but before the software
// reset executes, the IWDG is what delivers the request. Power-on (and
// shutdown wake) must discard stale SRAM contents instead.
constexpr std::uint32_t kFlagMagic = 0x00494150u; // "IAP"
constexpr std::uint8_t kFlagApp = 0xAAu;
constexpr std::uint8_t kFlagIap = 0x55u;

struct Flag
{
    std::uint32_t magic;
    std::uint8_t value;
    std::uint8_t reserved[3];
};

__attribute__((section(".noinit"))) volatile Flag g_flag;

bool reset_keeps_request() noexcept
{
    const std::uint8_t cause = sysreg::reset_status() & sysreg::kResetFlagMask;
    return cause == sysreg::kResetFlagSoftware ||
           cause == sysreg::kResetFlagWatchdog ||
           cause == sysreg::kResetFlagManual;
}

bool is_valid() noexcept
{
    return g_flag.magic == kFlagMagic && reset_keeps_request();
}

void set_flag(std::uint8_t value) noexcept
{
    g_flag.value = value;
    g_flag.magic = kFlagMagic;
}

} // namespace

bool requested() noexcept
{
    return is_valid() && g_flag.value == kFlagIap;
}

void app_ok() noexcept
{
    set_flag(kFlagApp);
}

void jump() noexcept
{
    set_flag(kFlagIap);
    sysreg::SafeAccess access;
    sysreg::reset_watchdog_ctrl() |= sysreg::kSoftwareReset;
    for (;;)
    {
    }
}

} // namespace ch572::iap
