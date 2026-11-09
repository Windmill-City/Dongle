#include "ch572/platform/flash.hpp"

#include "ch572/config.hpp"
#include "ch572/platform/registers.hpp"

namespace ch572::flash {
namespace {

// The controller exposes a small serial command interface. The erase and
// program sequences below are ported from the WCH reference driver and must
// run from RAM (.highcode): instruction fetch from flash is unavailable while
// the serial interface owns the array.
constexpr std::uint32_t kPageMask = 0xFFu;

constexpr std::uint8_t kCmdWriteEnable = 0x06;
constexpr std::uint8_t kCmdReadStatus = 0x05;
constexpr std::uint8_t kCmdProgram = 0x02;
constexpr std::uint8_t kCmdErase4K = 0x20;

inline CH572_HIGH_CODE void bus_delay() noexcept
{
    asm volatile("nop\n\tnop" ::: "memory");
}

CH572_HIGH_CODE void wait_ready() noexcept
{
    while ((flashreg::ctrl() & flashreg::kAct) != 0u)
    {
    }
}

CH572_HIGH_CODE void start_io(std::uint8_t value, bool twice) noexcept
{
    flashreg::ctrl() = 0;
    flashreg::ctrl() = flashreg::kEn | flashreg::kReadAction;
    bus_delay();
    flashreg::data8() = value;
    if (twice)
    {
        wait_ready();
        flashreg::data8() = value;
        wait_ready();
    }
}

CH572_HIGH_CODE void rom_end() noexcept
{
    wait_ready();
    flashreg::ctrl() = 0;
}

CH572_HIGH_CODE std::uint8_t rom_in() noexcept
{
    wait_ready();
    return flashreg::data8();
}

CH572_HIGH_CODE void rom_out(std::uint8_t value) noexcept
{
    wait_ready();
    flashreg::data8() = value;
}

CH572_HIGH_CODE void rom_address(std::uint8_t command, std::uint32_t address) noexcept
{
    start_io(kCmdWriteEnable, false);
    rom_end();

    start_io(command, false);
    for (int i = 0; i < 3; ++i)
    {
        rom_out(static_cast<std::uint8_t>(address >> 16));
        address <<= 8;
    }
}

CH572_HIGH_CODE bool rom_wait() noexcept
{
    rom_end();
    for (std::uint32_t i = 0; i < 0x80000u; ++i)
    {
        start_io(kCmdReadStatus, false);
        (void)rom_in();
        const std::uint8_t status = rom_in();
        rom_end();
        if ((status & 0x01u) == 0u)
        {
            return true;
        }
    }
    return false;
}

CH572_HIGH_CODE void begin_control() noexcept
{
    flashreg::glob_rom_cfg() |= flashreg::kRomCodeWe | flashreg::kRomCtrlEn;
    flashreg::ctrl() = flashreg::kReadAction;
    start_io(0xFF, true);
    rom_end();
}

CH572_HIGH_CODE void end_control() noexcept
{
    flashreg::glob_rom_cfg() &= flashreg::kRomCodeOfs;
}

CH572_HIGH_CODE bool program(std::uint32_t address, const std::uint8_t *data,
                             std::uint32_t words) noexcept
{
    begin_control();
    while (words != 0u)
    {
        rom_address(kCmdProgram, address);
        do
        {
            const std::uint32_t word = static_cast<std::uint32_t>(data[0]) |
                                       (static_cast<std::uint32_t>(data[1]) << 8) |
                                       (static_cast<std::uint32_t>(data[2]) << 16) |
                                       (static_cast<std::uint32_t>(data[3]) << 24);
            data += 4;
            flashreg::data32() = word;

            for (int shift = 0; shift < 4; ++shift)
            {
                wait_ready();
                flashreg::ctrl() = flashreg::kEn | flashreg::kReadAction | flashreg::kShift;
            }

            address += 4u;
            if (--words == 0u)
            {
                break;
            }
        } while ((address & kPageMask) != 0u);

        if (!rom_wait())
        {
            end_control();
            return false;
        }
    }

    rom_end();
    end_control();
    return true;
}

CH572_HIGH_CODE bool erase_range(std::uint32_t address, std::uint32_t length) noexcept
{
    begin_control();

    // erase() rounds the range to whole 4 KiB sectors, which is the only
    // granularity either firmware region uses.
    while (length >= kFlashSectorSize)
    {
        rom_address(kCmdErase4K, address);
        if (!rom_wait())
        {
            end_control();
            return false;
        }
        address += kFlashSectorSize;
        length -= kFlashSectorSize;
    }

    rom_end();
    end_control();
    return true;
}

} // namespace

CH572_HIGH_CODE bool erase(std::uint32_t address, std::uint32_t length) noexcept
{
    if (length == 0u)
    {
        return true;
    }
    if (address >= kFlashEnd || length > kFlashEnd - address)
    {
        return false;
    }

    const std::uint32_t start = address & ~(kFlashSectorSize - 1u);
    std::uint32_t total = (address - start) + length;
    total = (total + kFlashSectorSize - 1u) & ~(kFlashSectorSize - 1u);
    if (total > kFlashEnd - start)
    {
        return false;
    }

    sysreg::SafeAccess access;
    return erase_range(start, total);
}

CH572_HIGH_CODE bool write(std::uint32_t address, const void *buffer, std::uint32_t length) noexcept
{
    if (length == 0u)
    {
        return true;
    }
    if (buffer == nullptr || (address & 0x3u) != 0u ||
        address >= kFlashEnd || length > kFlashEnd - address)
    {
        return false;
    }

    const std::uint32_t words = length >> 2;
    if (words == 0u)
    {
        return true;
    }

    sysreg::SafeAccess access;
    return program(address, static_cast<const std::uint8_t *>(buffer), words);
}

} // namespace ch572::flash
