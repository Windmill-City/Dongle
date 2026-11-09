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

void blink(int count) noexcept
{
    for (int i = 0; i < count; ++i)
    {
        ch572::gpio::set(Pin::p7);
        ch572::systick::delay_ms(20u);
        ch572::gpio::clear(Pin::p7);
        ch572::systick::delay_ms(20u);
    }
}

} // namespace

int main()
{
    using namespace ch572;

    sysreg::disable_watchdog_reset();

#if defined(IAP_DEBUG)
    gpio::clear(Pin::p7);
    gpio::configure(Pin::p7, gpio::Mode::output_push_pull_5ma);

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
        systick::delay_ms(100u);
        gpio::clear(Pin::p7);
        systick::delay_ms(100u);
    }
#endif

    systick::start(6400u); // reset-default 6.4 MHz

    if (!iap::requested())
    {
        iap::request();
        usb::IspDevice::jump_to_app();
    }

    gpio::clear(Pin::p7);
    gpio::configure(Pin::p7, gpio::Mode::output_push_pull_5ma);

    // IAP mode: switch to 100 MHz and take over the USB pins.
    clock::set_hse_capacitance(clock::HseCapacitance::p8);
    clock::init_100mhz();
    systick::start(100000u); // 100 MHz
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
    constexpr std::uint32_t kHeartbeatTicks = 500u; // 500 ms
    std::uint32_t heartbeat = 0;
    bool led_on = false;

    for (;;)
    {
        isp.poll();

        if (++heartbeat >= kHeartbeatTicks)
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

        systick::delay_ms(1u);
    }
}
