#include <cstdint>

#include "ch572/hal/led.hpp"
#include "ch572/platform/clock.hpp"
#include "ch572/platform/registers.hpp"
#include "ch572/platform/systick.hpp"
#include "ch572/radio/rf_uart.hpp"
#include "ch572/usb/ch341.hpp"

int main()
{
    using namespace ch572;

    hal::Led led;
    led.init();
    led.on();

    // PA0/PA1 are the USB data pins; the two-wire debug interface must be off.
    sysreg::pin_alternate() &= ~sysreg::kPinDebugEnable;

    clock::set_hse_capacitance(clock::HseCapacitance::p18);
    clock::init_100mhz();
    systick::start_free_running();

    auto &usb = usb::Ch341Serial::instance();
    usb.init();

    rf_uart::RfUart rf(usb);
    usb.set_source(&rf);
    rf.init();

    constexpr std::uint32_t kBlinkTicks = 50u * 1000u * 1000u; // 500 ms at 100 MHz
    std::uint32_t last_blink = systick::now();
    bool usb_ready = false;

    for (;;)
    {
        usb.poll();

        const std::uint32_t now = systick::now();
        if (usb.enumerated())
        {
            if (!usb_ready)
            {
                usb_ready = true;
                led.on();
            }
        }
        else
        {
            usb_ready = false;
            if ((now - last_blink) >= kBlinkTicks)
            {
                last_blink = now;
                led.toggle();
            }
        }
    }
}
