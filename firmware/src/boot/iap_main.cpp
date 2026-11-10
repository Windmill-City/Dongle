#include "ch572/boot/iap_flag.hpp"
#include "ch572/boot/iap_update.hpp"
#include "ch572/config.hpp"
#include "ch572/platform/clock.hpp"
#include "ch572/platform/registers.hpp"
#include "ch572/platform/systick.hpp"
#include "ch572/usb/usb_device.hpp"

namespace {

[[noreturn]] void jump_to_app() noexcept
{
    ch572::usbreg::ctrl() = ch572::usbreg::kUcResetSie;
    ch572::sysreg::pin_alternate() &=
        ~(ch572::sysreg::kPinUsbEnable | ch572::sysreg::kUdpPullUpEnable);

    // Give the host time to notice the disconnect before handing over.
    ch572::systick::delay_ms(10u);

    using Entry = void (*)();
    reinterpret_cast<Entry>(ch572::kAppStart)();
    for (;;)
    {
    }
}

} // namespace

int main()
{
    using namespace ch572;

    sysreg::disable_watchdog_reset();

    systick::start(6400u); // reset-default 6.4 MHz

    // Boot the application unless it asked to stay in the bootloader (either
    // because it failed to confirm startup, because the host requested an
    // application upgrade, or because it requested a bootloader update).
    const bool boot_update = iap::update_requested();
    if (!boot_update && !iap::requested())
    {
        iap::request();
        jump_to_app();
    }

    // Application upgrade mode writes the application region. Bootloader
    // update mode first writes the staged image, then copies it to 0x0000 from
    // a RAM routine.
    clock::set_hse_capacitance(clock::HseCapacitance::p8);
    clock::init_100mhz();
    systick::start(100000u); // 100 MHz

    auto &usb = usb::UsbDevice::instance();
    usb.init();
    while (!usb.enumerated())
    {
        usb.poll();
    }

    sysreg::iwdg_start();
    if (boot_update)
    {
        iap::update_boot(usb);
    }
    iap::update_region(usb, kAppStart, kAppEnd);
}
