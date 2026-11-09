#pragma once

#include <cstdint>

#include "ch572/config.hpp"

namespace ch572::usb {

class IspDevice
{
public:
    static IspDevice &instance();

    void init() noexcept;
    CH572_HIGH_CODE bool poll() noexcept;
    static void jump_to_app() noexcept;

private:
    IspDevice() = default;

    static constexpr std::uint8_t kEp0Size = 64;
    static constexpr std::uint8_t kEp2Size = 64;
    static constexpr std::uint32_t kAppStart = 0x00002000u;
    static constexpr std::uint32_t kAppEnd = 0x0003C000u;
    static constexpr std::uint32_t kWriteChunk = 256;

    static constexpr std::uint8_t kCmdProgram = 0x80;
    static constexpr std::uint8_t kCmdErase = 0x81;
    static constexpr std::uint8_t kCmdVerify = 0x82;
    static constexpr std::uint8_t kCmdEnd = 0x83;

    struct SetupRequest
    {
        std::uint8_t request_type;
        std::uint8_t request;
        std::uint8_t value_l;
        std::uint8_t value_h;
        std::uint8_t index_l;
        std::uint8_t index_h;
        std::uint8_t length_l;
        std::uint8_t length_h;
    };
    static_assert(sizeof(SetupRequest) == 8, "USB setup packet is eight bytes");

    CH572_HIGH_CODE void handle_setup() noexcept;
    CH572_HIGH_CODE void handle_ep0_in() noexcept;
    CH572_HIGH_CODE void handle_command(std::uint8_t length) noexcept;
    CH572_HIGH_CODE void send_status(std::uint8_t status) noexcept;

    std::uint8_t m_config = 0;
    std::uint8_t m_setup_request = 0;
    std::uint16_t m_setup_length = 0;
    const std::uint8_t *m_descriptor = nullptr;

    alignas(4) std::uint8_t m_ep0_buffer[kEp0Size]{};
    alignas(4) std::uint8_t m_ep2_buffer[2 * kEp2Size]{};
    alignas(4) std::uint8_t m_write_buffer[kWriteChunk + kEp2Size]{};
    alignas(4) std::uint8_t m_command[kEp2Size]{};

    std::uint32_t m_flash_address = kAppStart;
    std::uint16_t m_write_length = 0;
};

} // namespace ch572::usb
