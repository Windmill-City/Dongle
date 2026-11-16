#include "ch572/platform/clock.hpp"
#include "ch572/platform/registers.hpp"
#include "ch572/radio/rf_uart.hpp"
#include "ch572/serial/uart.hpp"
#include "ch572/usb/usb_device.hpp"

using namespace ch572;

inline constexpr std::uint32_t kUsbDataPins = gpio::kPin0 | gpio::kPin1;

int                            main()
{
    sysreg::disable_watchdog_reset();
    clock::init();

    sysreg::pin_alternate() &= ~(sysreg::kPinDebugEnable | sysreg::kPinUsbEnable | sysreg::kUdpPullUpEnable);

    gpio::pa::pull_down() &= ~kUsbDataPins;
    gpio::pa::pull_up() |= kUsbDataPins;
    gpio::pa::dir() &= ~kUsbDataPins;

    systick::delay(100 * clock::kTicksPerUs);

    const bool host = (gpio::pa::pin() & kUsbDataPins) == 0;

    gpio::pa::pull_up() &= ~kUsbDataPins;

    usb::UsbDevice  usb;
    rf_uart::RfUart rf(host ? rf_uart::Role::master : rf_uart::Role::slave);

    enable_global_irq();

    if (host)
    {
        usb.set_source(rf.output());
        rf.attach(usb.host_input());
        usb.init();
    }
    else
    {
        serial::init();
    }

    rf.init();

    for (;;)
    {
        if (host)
        {
            usb.poll();
        }
        rf.poll();
    }
}
