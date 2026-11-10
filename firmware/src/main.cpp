#include <cstdint>

#include "ch572/platform/clock.hpp"
#include "ch572/platform/registers.hpp"
#include "ch572/platform/systick.hpp"
#include "ch572/radio/rf_uart.hpp"
#include "ch572/usb/usb_device.hpp"

namespace {

constexpr std::uint32_t kBlinkTicks = 50u * 1000u * 1000u; // 500 ms at 100 MHz
constexpr std::uint32_t kLedPin = 1u << 7;                 // PA7

} // namespace

int main()
{
    using namespace ch572;

    sysreg::disable_watchdog_reset();

    // PA7 push-pull, 5 mA.
    gpio::pull_down_drive_reg() &= ~kLedPin;
    gpio::clear_reg() = kLedPin;
    gpio::dir_reg() |= kLedPin;
    gpio::set_reg() = kLedPin;
    bool led_on = true;

    clock::set_hse_capacitance(clock::HseCapacitance::p8);
    clock::init_100mhz();
    systick::start(100000u);

    sysreg::iwdg_start();

    auto &usb = usb::UsbDevice::instance();
    usb.init();

    rf_uart::RfUart rf(usb);
    usb.set_source(&rf);
    rf.init();

    std::uint32_t last_blink = systick::now();

    for (;;)
    {
        usb.poll();
        sysreg::iwdg_feed();

        const std::uint32_t now = systick::now();
        if (usb.enumerated())
        {
            if (!led_on)
            {
                gpio::set_reg() = kLedPin;
                led_on = true;
            }
        }
        else if ((now - last_blink) >= kBlinkTicks)
        {
            last_blink = now;
            led_on = !led_on;
            if (led_on)
            {
                gpio::set_reg() = kLedPin;
            }
            else
            {
                gpio::clear_reg() = kLedPin;
            }
        }
    }
}
