#include "ch572/platform/clock.hpp"
#include "ch572/platform/registers.hpp"
#include "ch572/radio/rf_uart.hpp"
#include "ch572/usb/usb_device.hpp"

int main()
{
    using namespace ch572;

    sysreg::disable_watchdog_reset();
    clock::init_100mhz();
    sysreg::iwdg_start();

    auto &usb = usb::UsbDevice::instance();
    usb.init();

    rf_uart::RfUart rf(usb);
    usb.set_source(&rf);
    rf.init();

    for (;;)
    {
        usb.poll();
        sysreg::iwdg_feed();
    }
}
