#include "ch572/platform/systick.hpp"

#include "ch572/platform/registers.hpp"

namespace ch572::systick {
namespace {

constexpr std::uint32_t kCtlrStre = 1u << 3;
constexpr std::uint32_t kCtlrStclk = 1u << 2;
constexpr std::uint32_t kCtlrSte = 1u << 0;
constexpr std::uint32_t kReloadMax = 0xFFFFFFFFu;

} // namespace

void start_free_running() noexcept
{
    auto &timer = systick_regs();
    timer.cntl = 0;
    timer.cmpl = kReloadMax;
    timer.ctlr = kCtlrStre | kCtlrStclk | kCtlrSte;
}

std::uint32_t now() noexcept
{
    return systick_regs().cntl;
}

} // namespace ch572::systick
