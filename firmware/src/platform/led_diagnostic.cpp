#include "ch572/config.hpp"
#include "ch572/hal/led_diagnostic.hpp"
#include "ch572/platform/gpio.hpp"
#include "ch572/platform/registers.hpp"

#if defined(LED_SQUARE_TEST)

extern "C" CH572_INTERRUPT CH572_HIGH_CODE void SysTick_Handler(void)
{
    static bool on = false;

    if (on)
    {
        ch572::gpio::clear(ch572::gpio::Pin::p7);
    }
    else
    {
        ch572::gpio::set(ch572::gpio::Pin::p7);
    }
    on = !on;

    ch572::systick_regs().sr = 0;
}

#elif defined(LED_DIAGNOSTIC)

namespace {

volatile std::uint32_t g_stage = 1;
volatile std::uint32_t g_phase = 0;

constexpr std::uint32_t kMaxStage = 10;
constexpr std::uint32_t kGroupGap = 4;

} // namespace

namespace ch572::hal {

void set_led_stage(std::uint32_t stage) noexcept
{
    if (stage < 1)
    {
        stage = 1;
    }
    if (stage > kMaxStage)
    {
        stage = kMaxStage;
    }

    const auto state = irq_save();
    g_stage = stage;
    g_phase = 0;
    irq_restore(state);
}

} // namespace ch572::hal

extern "C" CH572_INTERRUPT CH572_HIGH_CODE void SysTick_Handler(void)
{
    const std::uint32_t stage = (g_stage > 3u) ? 3u : ((g_stage < 1u) ? 1u : g_stage);
    const std::uint32_t phase = g_phase;
    const std::uint32_t group = stage * 2u + kGroupGap;

    g_phase = (phase + 1u >= group) ? 0u : phase + 1u;

    const bool on = (phase < stage * 2u) && ((phase & 1u) == 0u);
    if (on)
    {
        ch572::gpio::set(ch572::gpio::Pin::p7);
    }
    else
    {
        ch572::gpio::clear(ch572::gpio::Pin::p7);
    }

    ch572::systick_regs().sr = 0;
}

#endif
