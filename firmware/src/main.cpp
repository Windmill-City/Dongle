#include <cstdint>

#include "ch572/boot/iap_flag.hpp"
#include "ch572/boot/iap_update.hpp"
#include "ch572/hal/led.hpp"
#include "ch572/platform/clock.hpp"
#include "ch572/platform/registers.hpp"
#include "ch572/platform/systick.hpp"
#include "ch572/radio/rf_uart.hpp"
#include "ch572/usb/ch341.hpp"

namespace {

constexpr std::uint32_t kBlinkTicks = 50u * 1000u * 1000u; // 500 ms at 100 MHz

} // namespace

int main()
{
    using namespace ch572;

    sysreg::disable_watchdog_reset();

    hal::Led led;
    led.init();
    led.on();

    // PA0/PA1 are the USB data pins; the two-wire debug interface must be off.
    sysreg::pin_alternate() &= ~sysreg::kPinDebugEnable;

    clock::set_hse_capacitance(clock::HseCapacitance::p8);
    clock::init_100mhz();
    systick::start(100000u);

    sysreg::iwdg_start_5s();

    auto &usb = usb::Ch341Serial::instance();
    usb.init();

    while (!usb.enumerated())
        usb.poll();
    sysreg::iwdg_feed();

    rf_uart::RfUart rf(usb);
    usb.set_source(&rf);
    rf.init();

    iap::app_ok();

    std::uint32_t last_blink = systick::now();

    for (;;)
    {
        usb.poll();
        sysreg::iwdg_feed();

        if (usb.iap_requested())
        {
            iap::jump();
        }

        if (usb.iap_update_requested())
        {
            rf::shutdown();
            disable_irq(Irq::BleBb);
            disable_irq(Irq::BleLle);
            iap::update_iap_region(usb);
        }

        const std::uint32_t now = systick::now();
        if (usb.enumerated())
        {
            led.on();
        }
        else if ((now - last_blink) >= kBlinkTicks)
        {
            last_blink = now;
            led.toggle();
        }
    }
}
