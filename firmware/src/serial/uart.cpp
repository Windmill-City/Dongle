#include "ch572/serial/uart.hpp"

#include "ch572/platform/clock.hpp"
#include "ch572/platform/registers.hpp"
#include "ch572/util/ring_buffer.hpp"

namespace ch572::serial
{
namespace
{

RingBuffer<kRxBufferSize>      g_rx_buffer;

inline constexpr std::size_t   kTxFifoDepth = 8;

bool enabled() noexcept
{
    return (uart::ier() & uart::kIerRecvReady) != 0;
}

inline constexpr std::uint16_t kTxPinSelect = 3;
inline constexpr std::uint16_t kRxPinSelect = 3;

void                           set_baud(std::uint32_t baud) noexcept
{
    if (baud < 1200u)
    {
        baud = 1200u;
    }

    std::uint32_t divisor = ((10u * clock::kSysClockHz / 8u / baud) + 5u) / 10u;
    if (divisor == 0)
    {
        divisor = 1;
    }

    uart::dll() = static_cast<std::uint8_t>(divisor & 0xFFu);
    uart::dlm() = static_cast<std::uint8_t>(divisor >> 8);
    uart::div() = 1;
}

std::uint8_t make_lcr(const LineCoding& coding) noexcept
{
    std::uint8_t data_bits = coding.data_bits;
    if (data_bits < 5 || data_bits > 8)
    {
        data_bits = 8;
    }

    std::uint8_t lcr = static_cast<std::uint8_t>(data_bits - 5);
    if (coding.stop_bits == kStopBits2)
    {
        lcr |= uart::kLcrStopBits;
    }

    switch (coding.parity)
    {
    case kParityOdd:
        lcr |= uart::kLcrParityEnable;
        break;

    case kParityEven:
        lcr |= uart::kLcrParityEnable | 0x10u;
        break;

    case kParityMark:
        lcr |= uart::kLcrParityEnable | 0x20u;
        break;

    case kParitySpace:
        lcr |= uart::kLcrParityEnable | 0x30u;
        break;

    default:
        break;
    }

    return lcr;
}

void drain_hw_rx() noexcept
{
    while (uart::rfc() != 0)
    {
        const std::uint8_t discarded = uart::rbr();
        (void)discarded;
    }
}

}

void init() noexcept
{
    sysreg::pin_alternate() &= ~(sysreg::kPinUsbEnable | sysreg::kUdpPullUpEnable);

    gpio::pa::pull_down() &= ~(gpio::kPin0 | gpio::kPin1);
    gpio::pa::pull_up() &= ~gpio::kPin0;
    gpio::pa::pull_up() |= gpio::kPin1;
    gpio::pa::dir() |= gpio::kPin0;
    gpio::pa::dir() &= ~gpio::kPin1;

    auto& alternate = sysreg::pin_alternate_high();
    alternate       = static_cast<std::uint16_t>((alternate & ~(sysreg::kPinUartRxdMask | sysreg::kPinUartTxdMask))
                                           | (kTxPinSelect << 3) | kRxPinSelect);

    set_baud(kBaudRate);

    uart::lcr() = uart::kLcrWordSizeMask;
    uart::fcr() = uart::kFcrFifoEnable;
    uart::ier() = 0;
    uart::mcr() |= uart::kMcrIrqOutput;

    enable_irq(Irq::Uart);
}

void configure(const LineCoding& coding) noexcept
{
    uart::ier() = 0;

    set_baud(coding.baud_rate);
    uart::lcr() = make_lcr(coding);

    drain_hw_rx();

    std::uint8_t scratch[32];
    while (g_rx_buffer.read(scratch, sizeof(scratch)) != 0)
    {
    }
}

void suspend() noexcept
{
    uart::ier() = 0;

    drain_hw_rx();
}

void resume() noexcept
{
    uart::fcr() = uart::kFcrFifoEnable;
    uart::ier() = uart::kIerTxdEnable | uart::kIerRecvReady;
    uart::mcr() |= uart::kMcrIrqOutput;
}

CH572_HIGH_CODE std::size_t read(std::uint8_t* dst, std::size_t max_length) noexcept
{
    return g_rx_buffer.read(dst, max_length);
}

void write(const std::uint8_t* src, std::size_t length) noexcept
{
    for (std::size_t index = 0; index < length; ++index)
    {
        while (uart::tfc() >= kTxFifoDepth)
        {
        }
        uart::thr() = src[index];
    }
}

std::size_t tx_space() noexcept
{
    const std::uint8_t queued = uart::tfc();
    return queued >= kTxFifoDepth ? 0 : kTxFifoDepth - queued;
}

void irq_handler() noexcept
{
    std::uint8_t      buffer[kTxFifoDepth];
    const std::size_t count = uart::rfc();
    std::size_t       index = 0;

    while (index < count && index < sizeof(buffer))
    {
        buffer[index++] = uart::rbr();
    }

    g_rx_buffer.write(buffer, index);
}

}

extern "C"
{
CH572_INTERRUPT void UART_IRQHandler(void)
{
    ch572::serial::irq_handler();
}
}
