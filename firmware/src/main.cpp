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

    // Release SWD and the USB PHY before sampling; otherwise PA0/PA1 are not
    // GPIO and the internal pull-ups cannot reach the pads.
    sysreg::pin_alternate() &= ~(sysreg::kPinDebugEnable | sysreg::kPinUsbEnable | sysreg::kUdpPullUpEnable);

    gpio::pa::pull_down() &= ~kUsbDataPins;
    gpio::pa::pull_up() |= kUsbDataPins;
    gpio::pa::dir() &= ~kUsbDataPins;

    systick::delay(100 * clock::kTicksPerUs);

    const bool host = (gpio::pa::pin() & kUsbDataPins) == 0;

    gpio::pa::pull_up() &= ~kUsbDataPins;

    auto&           usb = usb::UsbDevice::instance();
    rf_uart::RfUart rf(usb, host ? rf_uart::Role::master : rf_uart::Role::slave);

    enable_global_irq();

    if (host)
    {
        usb.init();
        usb.set_source(&rf);
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
