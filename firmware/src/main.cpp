#include <cstdint>

#include "ch572/boot/iap_flag.hpp"
#include "ch572/hal/led.hpp"
#include "ch572/hal/led_diagnostic.hpp"
#include "ch572/hal/uart.hpp"
#include "ch572/platform/clock.hpp"
#include "ch572/platform/flash.hpp"
#include "ch572/platform/registers.hpp"
#include "ch572/platform/systick.hpp"
#include "ch572/radio/rf_uart.hpp"
#include "ch572/radio/rfend.hpp"
#include "ch572/usb/ch341.hpp"

#if defined(USB_ONLY_TEST)
namespace {

// Simple USB-echo byte source: host bytes are pushed back to the host through
// Ch341Serial's source hook.
class LoopbackSource final : public ch572::usb::ByteSource
{
public:
    std::size_t read(std::uint8_t *dst, std::size_t max_length) override
    {
        std::size_t count = 0;
        while (count < max_length && m_tail != m_head)
        {
            dst[count++] = m_buffer[m_tail];
            m_tail = static_cast<std::uint16_t>((m_tail + 1u) % sizeof(m_buffer));
        }
        return count;
    }

    void push(const std::uint8_t *src, std::size_t length) noexcept
    {
        for (std::size_t i = 0; i < length; ++i)
        {
            const auto next = static_cast<std::uint16_t>((m_head + 1u) % sizeof(m_buffer));
            if (next == m_tail)
            {
                break;
            }
            m_buffer[m_head] = src[i];
            m_head = next;
        }
    }

private:
    std::uint8_t m_buffer[256]{};
    std::uint16_t m_head = 0;
    std::uint16_t m_tail = 0;
};

} // namespace
#endif

#if defined(USB_ONLY_TEST)
CH572_HIGH_CODE
#endif
int main()
{
    using namespace ch572;

    {
        sysreg::SafeAccess access;
        sysreg::reset_watchdog_ctrl() &= ~0x06u;
    }

#if defined(APP_HOLD_DEBUG)
    // Diagnostic build: keep PA0/PA1 as the debug interface (no USB, no RF) so
    // the bootloader hand-off can be observed over SWD. Blinks after clock init
    // and after the valid-flag write so each stage can be told apart.
    hal::Led hold_led;
    hold_led.init();
    hold_led.on();
    clock::set_hse_capacitance(clock::HseCapacitance::p8);
    clock::init_100mhz();
    // Flash self test with the same erase/write path the IAP uses. The result
    // word is left in RAM for a debugger: bit0=erase ok, bit1=write ok,
    // bit2=readback, upper nibble 0x1000 marks the test ran.
    *reinterpret_cast<volatile std::uint32_t *>(0x20002EF0u) = 0x11111111u;
    *reinterpret_cast<volatile std::uint32_t *>(0x20002E00u) = 0x000000AAu;
    std::uint32_t flash_result = 0x1000u;
    if (flash::erase(0x0003B000u, 4096u))
    {
        flash_result |= 0x01u;
    }
    if (flash::write(0x0003B000u, reinterpret_cast<const void *>(0x20002E00u), 4u))
    {
        flash_result |= 0x02u;
    }
    std::uint32_t flash_value = 0;
    flash::read(0x0003B000u, &flash_value, sizeof(flash_value));
    if (flash_value == 0x000000AAu)
    {
        flash_result |= 0x04u;
    }
    *reinterpret_cast<volatile std::uint32_t *>(0x20002EF0u) = flash_result;
    *reinterpret_cast<volatile std::uint32_t *>(0x20002EF4u) = 0x22222222u;
    iap::mark_app_valid();
    for (;;)
    {
        hold_led.toggle();
        for (volatile std::uint32_t i = 0; i < 4000000u; ++i)
        {
        }
    }
#endif

#if defined(UART_DEBUG)
#elif defined(LED_SQUARE_TEST)
    constexpr std::uint32_t kSquareTick = 50u * 1000u; // 500 us at 100 MHz
#elif defined(LED_DIAGNOSTIC)
    constexpr std::uint32_t kDiagnosticTick = 10u * 1000u * 1000u; // 100 ms at 100 MHz
#else
    constexpr std::uint32_t kBlinkTicks = 50u * 1000u * 1000u; // 500 ms at 100 MHz
#endif

    hal::Led led;
    led.init();
#if defined(LED_DIAGNOSTIC) || defined(LED_SQUARE_TEST)
    led.off();
#else
    led.on();
#endif

    // PA0/PA1 are the USB data pins; the two-wire debug interface must be off.
    sysreg::pin_alternate() &= ~sysreg::kPinDebugEnable;

#if defined(HSE_CAP_VALUE)
    constexpr auto kHseCap = static_cast<clock::HseCapacitance>(HSE_CAP_VALUE);
#else
    constexpr auto kHseCap = clock::HseCapacitance::p8;
#endif
    clock::set_hse_capacitance(kHseCap);
    clock::init_100mhz();
#if defined(APP_IAP)
    iap::mark_app_valid();

    // Arm the independent watchdog at 5 s. The 8-bit Fsys/131072 watchdog can
    // only reach ~0.33 s at 100 MHz, which is too short for USB enumeration
    // and radio init. 5 s still resets back to the ISP-only bootloader if the
    // app hangs.
    sysreg::iwdg_start_5s();
#endif

#if defined(UART_DEBUG)
    hal::uart::init(115200);
    hal::uart::puts("\r\nCH570Q UART debug\r\n");
#elif defined(LED_SQUARE_TEST)
    systick::start_periodic(kSquareTick);
#elif defined(LED_DIAGNOSTIC)
    systick::start_periodic(kDiagnosticTick);
#else
    systick::start_free_running();
#endif

    auto &usb = usb::Ch341Serial::instance();

#if defined(UART_DEBUG)
    (void)usb;

    rf::RoleConfig config{};
    config.process_cb = nullptr;
    config.process_mask = 0;

    hal::uart::puts("rf.init...\r\n");
    rf::role_init(&config);
    hal::uart::puts("rf.init done, probe=");
    hal::uart::put_u32(rfend::timer_probe());
    hal::uart::puts(" filter_timeout=");
    hal::uart::put_u32(rfend::rx_filter_timed_out() ? 1u : 0u);
    hal::uart::puts("\r\n");

    for (;;)
    {
    }
#else
    usb.init();

#if defined(USB_ONLY_TEST)
    // Minimal USB firmware: enumerate and echo, no radio.
    LoopbackSource loopback;
    usb.set_source(&loopback);
    std::uint32_t usb_only_last = systick::now();
    for (;;)
    {
        usb.poll();
        sysreg::iwdg_feed();
        std::uint8_t host_buffer[64];
        const auto host = usb.read_host(host_buffer, sizeof(host_buffer));
        if (host.status == usb::HostReadStatus::data)
        {
            loopback.push(host_buffer, host.length);
        }
        if (usb.enumerated())
        {
            usb_only_last = systick::now();
        }
        else if ((systick::now() - usb_only_last) > (5u * 100u * 1000u * 1000u))
        {
            iap::jump_to_iap();
        }
    }
#endif

#if defined(APP_IAP)
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
                iap::jump_to_iap();
            }
        }
    }
#endif

    rf_uart::RfUart rf(usb);
    usb.set_source(&rf);
    rf.init();
#if defined(LED_DIAGNOSTIC)
    hal::set_led_stage(ch572::rfend::rx_filter_timed_out() ? 2u : 3u);
#endif

#if !defined(LED_DIAGNOSTIC) && !defined(LED_SQUARE_TEST)
    std::uint32_t last_blink = systick::now();
    bool usb_ready = false;
#endif

#if defined(APP_IAP)
    // If the application cannot get its USB enumerated, reboot into the IAP
    // bootloader so the board is always reachable over USB without a power
    // cycle or the debug link.
    constexpr std::uint32_t kIapFallbackTicks = 5u * 100u * 1000u * 1000u;
    std::uint32_t last_enumerated = systick::now();
#endif

    for (;;)
    {
        usb.poll();
        sysreg::iwdg_feed();

#if defined(APP_IAP)
        if (usb.iap_requested())
        {
            iap::jump_to_iap();
        }
        if (usb.enumerated())
        {
            last_enumerated = systick::now();
        }
        else if ((systick::now() - last_enumerated) > kIapFallbackTicks)
        {
            iap::jump_to_iap();
        }
#endif

#if !defined(LED_DIAGNOSTIC) && !defined(LED_SQUARE_TEST)
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
#endif
    }
#endif
}
