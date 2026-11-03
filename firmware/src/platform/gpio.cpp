#include "ch572/platform/gpio.hpp"

#include "ch572/platform/registers.hpp"

namespace ch572::gpio {

void configure(Pin pin, Mode mode) noexcept
{
    const auto mask = static_cast<std::uint32_t>(pin);

    switch (mode)
    {
    case Mode::input_floating:
        pull_down_drive_reg() &= ~mask;
        pull_up_reg() &= ~mask;
        dir_reg() &= ~mask;
        break;

    case Mode::input_pull_up:
        pull_down_drive_reg() &= ~mask;
        pull_up_reg() |= mask;
        dir_reg() &= ~mask;
        break;

    case Mode::input_pull_down:
        pull_down_drive_reg() |= mask;
        pull_up_reg() &= ~mask;
        dir_reg() &= ~mask;
        break;

    case Mode::output_push_pull_5ma:
        pull_down_drive_reg() &= ~mask;
        dir_reg() |= mask;
        break;

    case Mode::output_push_pull_20ma:
        pull_down_drive_reg() |= mask;
        dir_reg() |= mask;
        break;
    }
}

void set(std::uint32_t pins) noexcept
{
    set_reg() = pins;
}

void clear(std::uint32_t pins) noexcept
{
    clear_reg() = pins;
}

} // namespace ch572::gpio
