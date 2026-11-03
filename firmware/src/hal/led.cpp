#include "ch572/hal/led.hpp"

#include "ch572/platform/gpio.hpp"
#include "ch572/platform/registers.hpp"

namespace ch572::hal {
namespace {

constexpr auto kLedPin = gpio::Pin::p7;

} // namespace

void Led::init() noexcept
{
    gpio::clear(kLedPin);
    gpio::configure(kLedPin, gpio::Mode::output_push_pull_5ma);
    m_on = false;
}

void Led::on() noexcept
{
    gpio::set(kLedPin);
    m_on = true;
}

void Led::off() noexcept
{
    gpio::clear(kLedPin);
    m_on = false;
}

void Led::toggle() noexcept
{
    if (m_on)
    {
        off();
    }
    else
    {
        on();
    }
}

} // namespace ch572::hal
