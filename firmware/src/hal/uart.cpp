#include "ch572/hal/uart.hpp"

#include "ch572/platform/gpio.hpp"
#include "ch572/platform/registers.hpp"

namespace ch572::hal::uart {
namespace {

constexpr std::uintptr_t kUartBase = 0x40003400u;
constexpr std::size_t kIer = 0x01;
constexpr std::size_t kFcr = 0x02;
constexpr std::size_t kLcr = 0x03;
constexpr std::size_t kThr = 0x08;
constexpr std::size_t kTfc = 0x0B;
constexpr std::size_t kDl = 0x0C;
constexpr std::size_t kDiv = 0x0E;

constexpr std::uint32_t kSysClock = 100000000u;
constexpr std::uint32_t kFifoSize = 8u;

volatile std::uint8_t &r8(std::size_t offset) noexcept
{
    return *reinterpret_cast<volatile std::uint8_t *>(kUartBase + offset);
}

volatile std::uint16_t &r16(std::size_t offset) noexcept
{
    return *reinterpret_cast<volatile std::uint16_t *>(kUartBase + offset);
}

} // namespace

void init(std::uint32_t baud) noexcept
{
    sysreg::pin_alternate() &= ~0x2000u;
    gpio::configure(gpio::Pin::p0, gpio::Mode::output_push_pull_5ma);
    gpio::configure(gpio::Pin::p1, gpio::Mode::input_pull_up);

    auto &alternate = sysreg::pin_alternate_high();
    alternate = static_cast<std::uint16_t>((alternate & ~0x3Fu) | (3u << 3) | 3u);

    const std::uint32_t divisor = ((10u * kSysClock / 8u / baud) + 5u) / 10u;
    r16(kDl) = static_cast<std::uint16_t>(divisor);
    r8(kFcr) = (2u << 6) | 0x01u;
    r8(kLcr) = 0x03u;
    r8(kIer) = 0x40u;
    r8(kDiv) = 1u;
}

void putc(char c) noexcept
{
    while (r8(kTfc) == kFifoSize)
    {
    }
    r8(kThr) = static_cast<std::uint8_t>(c);
}

void puts(const char *text) noexcept
{
    while (*text != '\0')
    {
        putc(*text++);
    }
}

void put_u32(std::uint32_t value) noexcept
{
    char buffer[10];
    std::size_t index = 0;

    if (value == 0)
    {
        putc('0');
        return;
    }

    while (value != 0 && index < sizeof(buffer))
    {
        buffer[index++] = static_cast<char>('0' + (value % 10u));
        value /= 10u;
    }

    while (index != 0)
    {
        putc(buffer[--index]);
    }
}

void put_hex32(std::uint32_t value) noexcept
{
    constexpr char kDigits[] = "0123456789ABCDEF";
    puts("0x");
    for (int shift = 28; shift >= 0; shift -= 4)
    {
        putc(kDigits[(value >> shift) & 0xFu]);
    }
}

} // namespace ch572::hal::uart
