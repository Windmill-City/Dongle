#include "ch572/boot/iap_update.hpp"

#include <cstdint>

#include "ch572/boot/iap_flag.hpp"
#include "ch572/config.hpp"
#include "ch572/platform/flash.hpp"
#include "ch572/platform/registers.hpp"
#include "ch572/platform/systick.hpp"
#include "ch572/util/ring_buffer.hpp"

namespace ch572::iap {
namespace {

// Wire format (all multi-byte fields little endian):
//
//   host -> device:  A5 | CMD | LEN_LO LEN_HI | PAYLOAD[LEN] | XOR
//   device -> host:  1 status byte
//
// XOR covers CMD, LEN_LO, LEN_HI and the payload. After the trigger the
// application sends kReady once; the host must wait for it before streaming
// frames, which keeps the trigger bytes out of the frame parser.
constexpr std::uint8_t kSof = 0xA5u;
constexpr std::uint8_t kReady = 0x5Au;

constexpr std::uint8_t kCmdErase = 0x01u;
constexpr std::uint8_t kCmdData = 0x02u;
constexpr std::uint8_t kCmdEnd = 0x04u;

constexpr std::uint8_t kStatusOk = 0x00u;
constexpr std::uint8_t kStatusFlash = 0x01u;
constexpr std::uint8_t kStatusFrame = 0x02u;
constexpr std::uint8_t kStatusRange = 0x03u;
constexpr std::uint8_t kStatusCrc = 0x04u;
constexpr std::uint8_t kStatusNotErased = 0x05u;

// Largest payload kept in RAM. The host sends 56-byte payloads (4-byte address
// plus 52 bytes of data), so this leaves room for retries and other hosts.
constexpr std::uint16_t kMaxPayload = 128u;

class ResponseSource final : public usb::ByteSource
{
public:
    void reset() noexcept { m_pending = false; }

    void push(std::uint8_t value) noexcept
    {
        m_value = value;
        m_pending = true;
    }

    std::size_t read(std::uint8_t *dst, std::size_t max_length) noexcept override
    {
        if (!m_pending || max_length == 0)
        {
            return 0;
        }
        *dst = m_value;
        m_pending = false;
        return 1;
    }

private:
    std::uint8_t m_value = 0;
    bool m_pending = false;
};

ResponseSource g_source;

std::uint32_t read_le32(const std::uint8_t *data) noexcept
{
    return static_cast<std::uint32_t>(data[0]) |
           (static_cast<std::uint32_t>(data[1]) << 8) |
           (static_cast<std::uint32_t>(data[2]) << 16) |
           (static_cast<std::uint32_t>(data[3]) << 24);
}

std::uint32_t crc32(const std::uint8_t *data, std::uint32_t length) noexcept
{
    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::uint32_t i = 0; i < length; ++i)
    {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit)
        {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

[[noreturn]] CH572_HIGH_CODE void copy_boot(std::uint32_t source,
                                             std::uint32_t length) noexcept
{
    // Interrupts stay off: after the first erase, no flash-resident handler
    // may run until the new image is in place.
    (void)irq_save();

    std::uint8_t buffer[256];
    sysreg::iwdg_feed();
    if (flash::erase(kIapStart, kIapEnd))
    {
        for (std::uint32_t offset = 0; offset < length; offset += sizeof(buffer))
        {
            const std::uint32_t remaining = length - offset;
            const std::uint32_t chunk =
                remaining > sizeof(buffer)
                    ? static_cast<std::uint32_t>(sizeof(buffer))
                    : remaining;
            wch_mcpy(buffer, reinterpret_cast<const void *>(source + offset), chunk);
            if (!flash::write(offset, buffer, chunk))
            {
                break;
            }
            sysreg::iwdg_feed();
        }
    }

    boot_ok();
    sysreg::software_reset();
}

[[noreturn]] void finish_boot(usb::UsbDevice &usb, std::uint32_t source,
                              std::uint32_t length) noexcept
{
    // Let the host read the final status byte before the USB device stops.
    for (int i = 0; i < 200; ++i)
    {
        usb.poll();
        sysreg::iwdg_feed();
        systick::delay_ms(1u);
    }
    copy_boot(source, length);
}

[[noreturn]] void finish(usb::UsbDevice &usb) noexcept
{
    // Give the host a moment to read the final status byte before the reset
    // disconnects the USB device. The application image has just been written
    // and CRC-checked, so confirm it to the bootloader and reboot.
    for (int i = 0; i < 200; ++i)
    {
        usb.poll();
        sysreg::iwdg_feed();
        systick::delay_ms(1u);
    }
    app_ok();
    sysreg::software_reset();
}

struct Session
{
    std::uint32_t start;
    std::uint32_t end;
    std::uint32_t erase_end;
    bool erased;
};

bool in_data_range(const Session &session, std::uint32_t address,
                   std::uint32_t length) noexcept
{
    return address >= session.start && address <= session.erase_end &&
           length <= session.erase_end - address;
}

void handle_command(usb::UsbDevice &usb, ResponseSource &out, std::uint8_t command,
                    const std::uint8_t *payload, std::uint16_t length,
                    Session &session, bool commit_boot) noexcept
{
    switch (command)
    {
    case kCmdErase:
    {
        if (length != 4u)
        {
            out.push(kStatusFrame);
            return;
        }
        const std::uint32_t image_length = read_le32(payload);
        if (image_length == 0u || image_length > session.end - session.start)
        {
            out.push(kStatusRange);
            return;
        }
        if (!flash::erase(session.start, image_length))
        {
            out.push(kStatusFlash);
            return;
        }
        session.erase_end = session.start + image_length;
        session.erased = true;
        out.push(kStatusOk);
        return;
    }

    case kCmdData:
    {
        if (length < 4u)
        {
            out.push(kStatusFrame);
            return;
        }
        const std::uint32_t address = read_le32(payload);
        const std::uint32_t data_length = static_cast<std::uint32_t>(length) - 4u;
        if (!session.erased)
        {
            out.push(kStatusNotErased);
            return;
        }
        if (data_length == 0u || (address & 0x3u) != 0u ||
            (data_length & 0x3u) != 0u || !in_data_range(session, address, data_length))
        {
            out.push(kStatusRange);
            return;
        }
        out.push(flash::write(address, payload + 4, data_length) ? kStatusOk
                                                                 : kStatusFlash);
        return;
    }

    case kCmdEnd:
    {
        if (length != 4u)
        {
            out.push(kStatusFrame);
            return;
        }
        if (!session.erased)
        {
            out.push(kStatusNotErased);
            return;
        }
        const auto *image = reinterpret_cast<const std::uint8_t *>(
            static_cast<std::uintptr_t>(session.start));
        const std::uint32_t actual = crc32(image, session.erase_end - session.start);
        if (actual != read_le32(payload))
        {
            out.push(kStatusCrc);
            return;
        }
        out.push(kStatusOk);
        if (commit_boot)
        {
            finish_boot(usb, session.start, session.erase_end - session.start);
        }
        finish(usb);
    }

    default:
        out.push(kStatusFrame);
        return;
    }
}

struct Receiver
{
    enum class State : std::uint8_t
    {
        sof,
        command,
        length_lo,
        length_hi,
        payload,
        checksum,
    };

    State state = State::sof;
    std::uint8_t command = 0;
    std::uint16_t length = 0;
    std::uint16_t index = 0;
    std::uint8_t checksum = 0;
    std::uint8_t payload[kMaxPayload]{};

    void feed(usb::UsbDevice &usb, ResponseSource &out, std::uint8_t value,
              Session &session, bool commit_boot) noexcept
    {
        switch (state)
        {
        case State::sof:
            if (value == kSof)
            {
                state = State::command;
            }
            break;

        case State::command:
            command = value;
            checksum = value;
            state = State::length_lo;
            break;

        case State::length_lo:
            length = value;
            checksum ^= value;
            state = State::length_hi;
            break;

        case State::length_hi:
            length = static_cast<std::uint16_t>(
                length | (static_cast<std::uint16_t>(value) << 8));
            checksum ^= value;
            index = 0;
            if (length > kMaxPayload)
            {
                // The host never sends oversized frames; report and resync.
                out.push(kStatusFrame);
                state = State::sof;
            }
            else
            {
                state = (length == 0u) ? State::checksum : State::payload;
            }
            break;

        case State::payload:
            payload[index++] = value;
            checksum ^= value;
            if (index >= length)
            {
                state = State::checksum;
            }
            break;

        case State::checksum:
            if (value == checksum)
            {
                handle_command(usb, out, command, payload, length, session,
                               commit_boot);
            }
            else
            {
                out.push(kStatusFrame);
            }
            state = State::sof;
            break;
        }
    }
};

[[noreturn]] void update_region_impl(usb::UsbDevice &usb, std::uint32_t start,
                                     std::uint32_t end, bool commit_boot) noexcept
{
    usb.set_source(&g_source);
    g_source.reset();

    // Drop the trigger and anything else buffered before the handshake.
    std::uint8_t scratch[32];
    while (usb.read_host(scratch, sizeof(scratch)) != 0u)
    {
    }

    g_source.push(kReady);

    Receiver receiver;
    Session session{start, end, start, false};

    for (;;)
    {
        usb.poll();
        sysreg::iwdg_feed();

        std::uint8_t chunk[32];
        const std::size_t length = usb.read_host(chunk, sizeof(chunk));
        for (std::size_t i = 0; i < length; ++i)
        {
            receiver.feed(usb, g_source, chunk[i], session, commit_boot);
        }
    }
}

} // namespace

[[noreturn]] void update_region(usb::UsbDevice &usb, std::uint32_t start,
                                std::uint32_t end) noexcept
{
    update_region_impl(usb, start, end, false);
}

[[noreturn]] void update_boot(usb::UsbDevice &usb) noexcept
{
    update_region_impl(usb, kStagingStart, kStagingEnd, true);
}

} // namespace ch572::iap
