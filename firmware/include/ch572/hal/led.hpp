#pragma once

#include <cstdint>

namespace ch572::hal {

class Led
{
public:
    void init() noexcept;
    void on() noexcept;
    void off() noexcept;
    void toggle() noexcept;

private:
    bool m_on = false;
};

} // namespace ch572::hal
