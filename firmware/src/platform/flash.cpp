#include "ch572/platform/flash.hpp"

#include <cstring>

extern "C" {
std::uint32_t FLASH_EEPROM_CMD(std::uint8_t cmd, std::uint32_t start, void *buffer, std::uint32_t length);
}

namespace ch572::flash {
namespace {

constexpr std::uint8_t kCmdErase = 0x01;
constexpr std::uint8_t kCmdWrite = 0x02;
constexpr std::uint8_t kCmdVerify = 0x03;

} // namespace

void read(std::uint32_t address, void *buffer, std::uint32_t length) noexcept
{
    std::memcpy(buffer, reinterpret_cast<const void *>(address), length);
}

bool erase(std::uint32_t address, std::uint32_t length) noexcept
{
    return FLASH_EEPROM_CMD(kCmdErase, address, nullptr, length) == 0;
}

bool write(std::uint32_t address, const void *buffer, std::uint32_t length) noexcept
{
    return FLASH_EEPROM_CMD(kCmdWrite, address, const_cast<void *>(buffer), length) == 0;
}

bool verify(std::uint32_t address, const void *buffer, std::uint32_t length) noexcept
{
    return FLASH_EEPROM_CMD(kCmdVerify, address, const_cast<void *>(buffer), length) == 0;
}

} // namespace ch572::flash
