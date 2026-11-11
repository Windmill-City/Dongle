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

// RB_UART_TXD = 3 selects PA0, RB_UART_RXD = 3 selects PA1.
inline constexpr std::uint16_t kTxPinSelect = 3;
inline constexpr std::uint16_t kRxPinSelect = 3;

void                           set_baud(std::uint32_t baud) noexcept
{
    // The divisor latch is 16 bits; reject settings the hardware cannot reach.
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

    // CH572 parity modes: 00 odd, 01 even, 10 mark, 11 space.
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

} // namespace

void init(std::uint32_t baud) noexcept
{
    // Release the USB PHY and its pull-up so PA0/PA1 fall back to GPIO
    // alternate function.
    sysreg::pin_alternate() &= ~(sysreg::kPinUsbEnable | sysreg::kUdpPullUpEnable);

    gpio::pa::pull_down() &= ~(gpio::kPin0 | gpio::kPin1);
    gpio::pa::pull_up() &= ~gpio::kPin0;
    gpio::pa::pull_up() |= gpio::kPin1;
    gpio::pa::dir() |= gpio::kPin0;
    gpio::pa::dir() &= ~gpio::kPin1;

    auto& alternate = sysreg::pin_alternate_high();
    alternate       = static_cast<std::uint16_t>((alternate & ~(sysreg::kPinUartRxdMask | sysreg::kPinUartTxdMask))
                                           | (kTxPinSelect << 3) | kRxPinSelect);

    set_baud(baud);

    // Eight data bits, one stop bit, no parity; four-byte RX FIFO trigger.
    uart::lcr() = uart::kLcrWordSizeMask;
    uart::fcr() = (2u << 6) | uart::kFcrFifoEnable;
    uart::ier() = uart::kIerTxdEnable | uart::kIerRecvReady;
    uart::mcr() |= uart::kMcrIrqOutput;

    enable_irq(Irq::Uart);
}

void configure(const LineCoding& coding) noexcept
{
    set_baud(coding.baud_rate);
    uart::lcr() = make_lcr(coding);

    // Only the radio interrupt changes the line settings, and same-priority
    // PFIC interrupts do not nest, so the receive ring cannot move while it is
    // drained here.
    std::uint8_t scratch[32];
    while (g_rx_buffer.read(scratch, sizeof(scratch)) != 0)
    {
    }
}

CH572_HIGH_CODE std::size_t read(std::uint8_t* dst, std::size_t max_length) noexcept
{
    return g_rx_buffer.read(dst, max_length);
}

void write(const std::uint8_t* src, std::size_t length) noexcept
{
    if (src == nullptr)
    {
        return;
    }

    for (std::size_t index = 0; index < length; ++index)
    {
        while (uart::tfc() >= kTxFifoDepth)
        {
        }
        uart::thr() = src[index];
    }
}

void irq_handler() noexcept
{
    // Reading RBR until the FIFO is empty also clears the RX-ready and
    // receive-timeout interrupt sources.
    std::uint8_t      buffer[kTxFifoDepth];
    const std::size_t count = uart::rfc();
    std::size_t       index = 0;

    while (index < count && index < sizeof(buffer))
    {
        buffer[index++] = uart::rbr();
    }

    g_rx_buffer.write(buffer, index);
}

} // namespace ch572::serial

extern "C"
{
CH572_INTERRUPT void UART_IRQHandler(void)
{
    ch572::serial::irq_handler();
}
}
