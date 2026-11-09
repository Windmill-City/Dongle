#include "ch572/boot/iap_flag.hpp"

#include <cstdint>

#include "ch572/platform/flash.hpp"
#include "ch572/platform/registers.hpp"

namespace ch572::iap {
namespace {

constexpr std::uint32_t kFlagAddress = 0x0003B000u;
constexpr std::uint32_t kFlagPageSize = 4096u;
constexpr std::uint8_t kFlagApp = 0xAAu;
constexpr std::uint8_t kFlagIap = 0x55u;

void set_flag(std::uint8_t value) noexcept
{
    alignas(4) std::uint8_t data[4] = {};
    flash::read(kFlagAddress, data, sizeof(data));
    if (data[0] == value)
    {
        return;
    }

    flash::erase(kFlagAddress, kFlagPageSize);
    data[0] = value;
    data[1] = 0;
    data[2] = 0;
    data[3] = 0;
    flash::write(kFlagAddress, data, sizeof(data));
}

} // namespace

bool is_iap_requested() noexcept
{
    return *reinterpret_cast<const volatile std::uint8_t *>(kFlagAddress) == kFlagIap;
}

void mark_app_valid() noexcept
{
    set_flag(kFlagApp);
}

void jump_to_iap() noexcept
{
    set_flag(kFlagIap);
    sysreg::SafeAccess access;
    sysreg::reset_watchdog_ctrl() |= sysreg::kSoftwareReset;
    for (;;)
    {
    }
}

} // namespace ch572::iap
