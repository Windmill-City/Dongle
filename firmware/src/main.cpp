#include <cstdint>

#include "ch572/boot/iap_flag.hpp"
#include "ch572/hal/led.hpp"
#include "ch572/platform/clock.hpp"
#include "ch572/platform/registers.hpp"
#include "ch572/platform/systick.hpp"
#include "ch572/radio/rf_uart.hpp"
#include "ch572/usb/ch341.hpp"

int main()
{
    using namespace ch572;

    {
        sysreg::SafeAccess access;
        sysreg::reset_watchdog_ctrl() &= ~0x06u;
    }

    constexpr std::uint32_t kBlinkTicks = 50u * 1000u * 1000u; // 500 ms at 100 MHz

    hal::Led led;
    led.init();
    led.on();

    // PA0/PA1 are the USB data pins; the two-wire debug interface must be off.
    sysreg::pin_alternate() &= ~sysreg::kPinDebugEnable;

    clock::set_hse_capacitance(clock::HseCapacitance::p8);
    clock::init_100mhz();
    iap::app_ok();

    // Arm the independent watchdog at 5 s. The 8-bit Fsys/131072 watchdog can
    // only reach ~0.33 s at 100 MHz, which is too short for USB enumeration
    // and radio init. 5 s still resets back to the ISP-only bootloader if the
    // app hangs.
    sysreg::iwdg_start_5s();

    systick::start(100000u);

    auto &usb = usb::Ch341Serial::instance();
    usb.init();

    // Wait for enumeration before touching the radio. Polling USB in a tight
    // loop here also gives the setup packets a much better chance than being
    // deferred behind the (flash-resident, slow) RF init. If the device never
    // enumerates, return to the IAP bootloader so the board stays reachable.
    {
        const std::uint32_t enum_start = systick::now();
        while (!usb.enumerated())
        {
            usb.poll();
            sysreg::iwdg_feed();
            if ((systick::now() - enum_start) > (5u * 100u * 1000u * 1000u))
            {
                iap::jump();
            }
        }
    }

    rf_uart::RfUart rf(usb);
    usb.set_source(&rf);
    rf.init();

    std::uint32_t last_blink = systick::now();
    bool usb_ready = false;

    // If the application cannot get its USB enumerated, reboot into the IAP
    // bootloader so the board is always reachable over USB without a power
    // cycle or the debug link.
    constexpr std::uint32_t kIapFallbackTicks = 5u * 100u * 1000u * 1000u;
    std::uint32_t last_enumerated = systick::now();

    for (;;)
    {
        usb.poll();
        sysreg::iwdg_feed();

        if (usb.iap_requested())
        {
            iap::jump();
        }
        if (usb.enumerated())
        {
            last_enumerated = systick::now();
        }
        else if ((systick::now() - last_enumerated) > kIapFallbackTicks)
        {
            iap::jump();
        }

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
