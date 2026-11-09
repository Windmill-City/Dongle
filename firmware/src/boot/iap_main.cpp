#include <cstdint>

#include "ch572/boot/iap_flag.hpp"
#include "ch572/config.hpp"
#include "ch572/platform/clock.hpp"
#include "ch572/platform/gpio.hpp"
#include "ch572/platform/registers.hpp"
#include "ch572/platform/systick.hpp"
#include "ch572/usb/isp.hpp"

namespace {

using ch572::gpio::Pin;

// Debug builds stay resident with the SWD pins untouched. The debugger asks
// the bootloader to start the application by writing kDebugJumpMagic here.
constexpr std::uint32_t kDebugJumpAddress = 0x20002F00u;
constexpr std::uint32_t kDebugJumpMagic = 0x5AA55AA5u;

CH572_HIGH_CODE void delay_ticks(std::uint32_t ticks) noexcept
{
    asm volatile(
        "1:\n\t"
        "addi %0, %0, -1\n\t"
        "bnez %0, 1b\n\t"
        : "+r"(ticks)
        :
        : "memory");
}

void delay_ms(std::uint32_t ms) noexcept
{
    constexpr std::uint32_t kTicksPerMs = 100000u;
    const std::uint32_t ticks = ms * kTicksPerMs;
    const std::uint32_t start = ch572::systick::now();
    while ((start - ch572::systick::now()) < ticks)
    {
    }
}

void blink(int count) noexcept
{
    for (int i = 0; i < count; ++i)
    {
        ch572::gpio::set(Pin::p7);
        delay_ticks(800000u);
        ch572::gpio::clear(Pin::p7);
        delay_ticks(800000u);
    }
}

} // namespace

// Run the whole bootloader from RAM. Executing the ISP poll loop from flash
// at 100 MHz is far too slow: the USB device is polled late and the host
// reports a descriptor failure, and the heartbeat blink stretches to seconds.
CH572_HIGH_CODE int main()
{
    using namespace ch572;

#if defined(IAP_GPIO_TEST)
    // Minimal bring-up test: no clock init, no safe access, no interrupts.
    // Drive PA7 (LED, high = on) directly and toggle at ~1 Hz on the
    // reset-default 6.4 MHz clock so even a multimeter can see it.
    *reinterpret_cast<volatile std::uint32_t *>(0x400010A0u) |= 0x80u; // PA_DIR
    *reinterpret_cast<volatile std::uint32_t *>(0x400010B4u) |= 0x80u; // PA_PD_DRV
    for (;;)
    {
        *reinterpret_cast<volatile std::uint32_t *>(0x400010B8u) = 0x80u; // PA_SET
        delay_ticks(2000000u);
        *reinterpret_cast<volatile std::uint32_t *>(0x400010ACu) = 0x80u; // PA_CLR
        delay_ticks(2000000u);
    }
#endif

#if defined(IAP_BLINK_TEST)
    {
        sysreg::SafeAccess access;
        sysreg::reset_watchdog_ctrl() &= ~0x06u;
    }

    clock::set_hse_capacitance(clock::HseCapacitance::p8);
    clock::init_100mhz();

    gpio::clear(Pin::p7);
    gpio::configure(Pin::p7, gpio::Mode::output_push_pull_20ma);
    for (;;)
    {
        *reinterpret_cast<volatile std::uint32_t *>(0x400010B8u) = 0x80u;
        delay_ticks(10000000u);
        *reinterpret_cast<volatile std::uint32_t *>(0x400010ACu) = 0x80u;
        delay_ticks(10000000u);
    }
#endif

    {
        sysreg::SafeAccess access;
        sysreg::reset_watchdog_ctrl() &= ~0x06u;
    }

    gpio::clear(Pin::p7);
    gpio::configure(Pin::p7, gpio::Mode::output_push_pull_5ma);

#if defined(IAP_DEBUG)
    // Debug build: never leave on its own and never touch PA0/PA1, so a broken
    // application can always be recovered over SWD. The application is started
    // only when the debugger writes the jump magic to kDebugJumpAddress.
    volatile std::uint32_t *const jump_request =
        reinterpret_cast<volatile std::uint32_t *>(kDebugJumpAddress);
    for (;;)
    {
        if (*jump_request == kDebugJumpMagic)
        {
            *jump_request = 0;
            usb::IspDevice::jump_to_app();
        }
        gpio::set(Pin::p7);
        delay_ticks(200000u);
        gpio::clear(Pin::p7);
        delay_ticks(200000u);
    }
#endif

#if defined(IAP_LED_TEST)
    for (;;)
    {
        gpio::set(Pin::p7);
        delay_ticks(10000000u);
        gpio::clear(Pin::p7);
        delay_ticks(10000000u);
    }
#endif

    // Stage 1: four blinks at the reset-default clock. The debug pins are
    // still available here, which also gives the programmer a recovery window.
    blink(4);
    gpio::set(Pin::p7);

#if !defined(IAP_TEST)
    // Leave for the application before touching the clock or the debug pins.
    // The app runs its own SetSysClock(); initialising the PLL here and then
    // again in the app hangs the second PLL setup.
    if (!iap::is_iap_requested())
    {
        usb::IspDevice::jump_to_app();
    }
#endif

    // IAP mode: switch to 100 MHz and take over the USB pins.
    clock::set_hse_capacitance(clock::HseCapacitance::p8);
    clock::init_100mhz();
    gpio::clear(Pin::p7);
    gpio::configure(Pin::p7, gpio::Mode::output_push_pull_5ma);
    sysreg::pin_alternate() &= ~sysreg::kPinDebugEnable;

    // Stage 2: two blinks mean the 100 MHz clock init returned.
    blink(2);
    gpio::set(Pin::p7);

    auto &isp = usb::IspDevice::instance();
    isp.init();

    // Stage 3: three blinks mean USB ISP init returned.
    blink(3);

    // Once the application has requested an upgrade, stay in the ISP until the
    // host finishes with the End command; only a reset can leave earlier.
    constexpr std::uint32_t kTimeoutTicks = 0xFFFFFFFFu;
    std::uint32_t idle_ticks = 0;
    std::uint32_t heartbeat = 0;
    bool led_on = false;

    for (;;)
    {
        if (isp.poll())
        {
            idle_ticks = 0;
        }
        else if (++idle_ticks > kTimeoutTicks)
        {
            usb::IspDevice::jump_to_app();
        }

        if (++heartbeat >= 5000u)
        {
            heartbeat = 0;
            led_on = !led_on;
            if (led_on)
            {
                gpio::set(Pin::p7);
            }
            else
            {
                gpio::clear(Pin::p7);
            }
        }

        for (volatile std::uint32_t i = 0; i < 2000u; ++i)
        {
            nop();
        }
    }
}






