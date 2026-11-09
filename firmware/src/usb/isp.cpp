#include "ch572/usb/isp.hpp"

#include <cstring>

#include "ch572/platform/flash.hpp"
#include "ch572/platform/registers.hpp"
#include "ch572/platform/systick.hpp"
#include "ch572/usb/usb_common.hpp"

namespace ch572::usb {
namespace {

constexpr std::uint8_t kDeviceDescriptor[] = {
    0x12, 0x01, 0x10, 0x01, 0xFF, 0x80, 0x55, 0x40,
    0x48, 0x43, 0xE0, 0x55, 0x00, 0x01, 0x00, 0x00,
    0x00, 0x01,
};

constexpr std::uint8_t kConfigDescriptor[] = {
    0x09, 0x02, 0x20, 0x00, 0x01, 0x01, 0x00, 0x80, 0x32,
    0x09, 0x04, 0x00, 0x00, 0x02, 0xFF, 0x80, 0x55, 0x00,
    0x07, 0x05, 0x82, 0x02, 0x40, 0x00, 0x00,
    0x07, 0x05, 0x02, 0x02, 0x40, 0x00, 0x00,
};

constexpr std::uint8_t kLangDescriptor[] = {0x04, 0x03, 0x09, 0x04};
constexpr std::uint8_t kManufacturerDescriptor[] = {
    0x0E, 0x03, 'w', 0, 'c', 0, 'h', 0, '.', 0, 'c', 0, 'n', 0,
};
constexpr std::uint8_t kProductDescriptor[] = {
    0x0C, 0x03, 'C', 0, 'H', 0, '5', 0, '7', 0, 'x', 0,
};

constexpr DescriptorSet kDescriptors = {
    kDeviceDescriptor,
    kConfigDescriptor,
    nullptr,
    {kLangDescriptor, kManufacturerDescriptor, kProductDescriptor, nullptr},
};

inline std::uint32_t read_le32(const std::uint8_t *data) noexcept
{
    return static_cast<std::uint32_t>(data[0]) |
           (static_cast<std::uint32_t>(data[1]) << 8) |
           (static_cast<std::uint32_t>(data[2]) << 16) |
           (static_cast<std::uint32_t>(data[3]) << 24);
}

} // namespace

IspDevice &IspDevice::instance()
{
    static IspDevice device;
    return device;
}

void IspDevice::init() noexcept
{
    usbreg::ctrl() = 0;
    usbreg::ep4_1_mod() = 0;
    usbreg::ep2_3_mod() = usbreg::kUep2RxEn | usbreg::kUep2TxEn;

    usbreg::ep0_dma() = static_cast<std::uint16_t>(reinterpret_cast<std::uintptr_t>(&m_ep0_buffer[0]));
    usbreg::ep2_dma() = static_cast<std::uint16_t>(reinterpret_cast<std::uintptr_t>(&m_ep2_buffer[0]));

    usbreg::ep0_ctrl() = usbreg::kUepRResAck | usbreg::kUepTResNak;
    usbreg::ep2_ctrl() = usbreg::kUepRResAck | usbreg::kUepTResNak | usbreg::kUepAutoTog;

    usbreg::dev_addr() = 0;
    usbreg::ctrl() = usbreg::kUcDevPuEn | usbreg::kUcIntBusy | usbreg::kUcDmaEn;
    sysreg::pin_alternate() |= sysreg::kPinUsbEnable | sysreg::kUdpPullUpEnable;
    usbreg::int_fg() = 0xFF;
    usbreg::int_en() = 0;
    usbreg::dev_ctrl() = usbreg::kUdPdDis | usbreg::kUdPortEn;

    m_config = 0;
    m_setup_request = 0;
    m_setup_length = 0;
    m_descriptor = nullptr;
    m_flash_address = kAppStart;
    m_write_length = 0;
}

void IspDevice::poll() noexcept
{
    const std::uint8_t flags = usbreg::int_fg();

    if ((flags & usbreg::kUifTransfer) != 0)
    {
        if ((flags & usbreg::kUifNak) == 0)
        {
            switch (usbreg::int_st() & (usbreg::kMaskUisToken | usbreg::kMaskUisEndp))
            {
            case usbreg::kUisTokenIn | 2:
                usbreg::ep2_ctrl() = (usbreg::ep2_ctrl() & ~usbreg::kMaskUepTRes) | usbreg::kUepTResNak;
                break;

            case usbreg::kUisTokenOut | 2:
                if ((usbreg::int_st() & usbreg::kUisTogOk) != 0)
                {
                    const std::uint8_t length = usbreg::rx_len();
                    std::memcpy(m_command, m_ep2_buffer, length);
                    handle_command(length);
                }
                break;

            case usbreg::kUisTokenIn:
                handle_ep0_in();
                break;

            case usbreg::kUisTokenOut:
                usbreg::ep0_ctrl() = usbreg::kUepRResAck | usbreg::kUepTResNak;
                break;

            default:
                break;
            }
            usbreg::int_fg() = usbreg::kUifTransfer;
        }

        if ((usbreg::int_st() & usbreg::kUisSetupAct) != 0)
        {
            handle_setup();
        }
        usbreg::int_fg() = usbreg::kUifTransfer;
        return;
    }

    if ((flags & usbreg::kUifBusReset) != 0)
    {
        usbreg::dev_addr() = 0;
        usbreg::ep0_ctrl() = usbreg::kUepRResAck | usbreg::kUepTResNak;
        usbreg::ep2_ctrl() = usbreg::kUepRResAck | usbreg::kUepTResNak | usbreg::kUepAutoTog;
        usbreg::int_fg() = usbreg::kUifBusReset;
        return;
    }

    if ((flags & usbreg::kUifSuspend) != 0)
    {
        usbreg::int_fg() = usbreg::kUifSuspend;
        return;
    }

    usbreg::int_fg() = flags;
}

void IspDevice::handle_setup() noexcept
{
    auto *setup = reinterpret_cast<SetupRequest *>(m_ep0_buffer);

    usbreg::ep0_ctrl() = usbreg::kUepRTog | usbreg::kUepTTog |
                         usbreg::kUepRResAck | usbreg::kUepTResNak;

    m_setup_request = setup->request;
    m_setup_length = static_cast<std::uint16_t>(setup->length_l) |
                     (static_cast<std::uint16_t>(setup->length_h) << 8);

    bool error = false;
    std::uint8_t data_length = 0;

    if ((setup->request_type & kRequestTypeMask) != kRequestTypeStandard)
    {
        error = true;
    }
    else
    {
        switch (setup->request)
        {
        case kReqGetDescriptor:
        {
            const std::uint8_t *descriptor =
                find_descriptor(kDescriptors, setup->value_h, setup->value_l);
            if (descriptor == nullptr)
            {
                error = true;
                break;
            }

            // bLength is the first byte of every USB descriptor.
            m_descriptor = descriptor;
            if (m_setup_length > descriptor[0])
            {
                m_setup_length = descriptor[0];
            }
            data_length = (m_setup_length >= kEp0Size) ? kEp0Size : static_cast<std::uint8_t>(m_setup_length);
            std::memcpy(m_ep0_buffer, m_descriptor, data_length);
            m_descriptor += data_length;
            m_setup_length = static_cast<std::uint16_t>(m_setup_length - data_length);
            break;
        }

        case kReqSetAddress:
            m_setup_length = setup->value_l;
            break;

        case kReqGetConfiguration:
            m_ep0_buffer[0] = m_config;
            m_setup_length = (m_setup_length > 1) ? 1 : m_setup_length;
            data_length = static_cast<std::uint8_t>(m_setup_length);
            m_setup_length = 0;
            break;

        case kReqSetConfiguration:
            m_config = setup->value_l;
            break;

        case kReqClearFeature:
            if (((setup->request_type & kRecipientMask) == kRecipientEndpoint))
            {
                if (setup->index_l == 0x82)
                {
                    usbreg::ep2_ctrl() = (usbreg::ep2_ctrl() & ~(usbreg::kUepTTog | usbreg::kMaskUepTRes)) |
                                         usbreg::kUepTResNak;
                }
                else if (setup->index_l == 0x02)
                {
                    usbreg::ep2_ctrl() = (usbreg::ep2_ctrl() & ~(usbreg::kUepRTog | usbreg::kMaskUepRRes)) |
                                         usbreg::kUepRResAck;
                }
                else
                {
                    error = true;
                }
            }
            else
            {
                error = true;
            }
            break;

        case kReqGetInterface:
            m_ep0_buffer[0] = 0;
            m_setup_length = (m_setup_length > 1) ? 1 : m_setup_length;
            data_length = static_cast<std::uint8_t>(m_setup_length);
            m_setup_length = 0;
            break;

        case kReqGetStatus:
            m_ep0_buffer[0] = 0;
            m_ep0_buffer[1] = 0;
            m_setup_length = (m_setup_length > 2) ? 2 : m_setup_length;
            data_length = static_cast<std::uint8_t>(m_setup_length);
            m_setup_length = 0;
            break;

        default:
            error = true;
            break;
        }
    }

    if (error)
    {
        usbreg::ep0_t_len() = 0;
        usbreg::ep0_ctrl() = usbreg::kUepRTog | usbreg::kUepTTog |
                             usbreg::kUepRResStall | usbreg::kUepTResStall;
        return;
    }

    // Data-stage requests filled data_length above; control requests left it 0.
    usbreg::ep0_t_len() = data_length;
    usbreg::ep0_ctrl() = usbreg::kUepRTog | usbreg::kUepTTog |
                         usbreg::kUepRResAck | usbreg::kUepTResAck;
}

void IspDevice::handle_ep0_in() noexcept
{
    if (m_setup_request == kReqGetDescriptor)
    {
        if (m_setup_length != 0)
        {
            const std::uint8_t length = (m_setup_length >= kEp0Size) ? kEp0Size : static_cast<std::uint8_t>(m_setup_length);
            std::memcpy(m_ep0_buffer, m_descriptor, length);
            m_descriptor += length;
            m_setup_length = static_cast<std::uint16_t>(m_setup_length - length);
            usbreg::ep0_t_len() = length;
            usbreg::ep0_ctrl() ^= usbreg::kUepTTog;
            return;
        }
    }
    else if (m_setup_request == kReqSetAddress)
    {
        usbreg::dev_addr() = static_cast<std::uint8_t>((usbreg::dev_addr() & usbreg::kUdaGpBit) |
                                                       (m_setup_length & 0x7Fu));
        usbreg::ep0_t_len() = 0;
        usbreg::ep0_ctrl() = usbreg::kUepRResAck | usbreg::kUepTResNak;
        return;
    }

    usbreg::ep0_t_len() = 0;
    usbreg::ep0_ctrl() = usbreg::kUepRResAck | usbreg::kUepTResNak;
}

void IspDevice::handle_command(std::uint8_t length) noexcept
{
    if (length < 2)
    {
        send_status(0xFE);
        return;
    }

    switch (m_command[0])
    {
    case kCmdProgram:
    {
        const std::uint8_t count = m_command[1];
        std::uint8_t status = 0;

        if (count == 0)
        {
            if (m_write_length != 0)
            {
                const std::uint16_t aligned = static_cast<std::uint16_t>((m_write_length + 3u) & ~3u);
                status = flash::write(m_flash_address, m_write_buffer, aligned) ? 0 : 1;
                m_write_length = 0;
            }
        }
        else
        {
            std::memcpy(m_write_buffer + m_write_length, m_command + 2, count);
            m_write_length = static_cast<std::uint16_t>(m_write_length + count);
            if (m_write_length >= kWriteChunk)
            {
                status = flash::write(m_flash_address, m_write_buffer, kWriteChunk) ? 0 : 1;
                m_flash_address += kWriteChunk;
                m_write_length = static_cast<std::uint16_t>(m_write_length - kWriteChunk);
                std::memmove(m_write_buffer, m_write_buffer + kWriteChunk, m_write_length);
            }
        }
        send_status(status);
        break;
    }

    case kCmdErase:
    {
        const std::uint32_t address = read_le32(&m_command[2]);
        std::uint8_t status = 0xFE;
        if (address == kAppStart)
        {
            status = flash::erase(kAppStart, kAppEnd - kAppStart) ? 0 : 1;
            m_write_length = 0;
            m_flash_address = kAppStart;
        }
        send_status(status);
        break;
    }

    case kCmdVerify:
    {
        const std::uint32_t address = read_le32(&m_command[2]);
        const std::uint8_t count = m_command[1];
        const bool ok = flash::verify(address, m_command + 6, count);
        send_status(ok ? 0 : 1);
        break;
    }

    case kCmdEnd:
        send_status(0);
        jump_to_app();
        break;

    default:
        send_status(0xFE);
        break;
    }
}

void IspDevice::send_status(std::uint8_t status) noexcept
{
    m_ep2_buffer[kEp2Size] = status;
    m_ep2_buffer[kEp2Size + 1] = 0;
    usbreg::ep2_t_len() = 2;
    usbreg::ep2_ctrl() = (usbreg::ep2_ctrl() & ~usbreg::kMaskUepTRes) | usbreg::kUepTResAck;
}

void IspDevice::jump_to_app() noexcept
{
    usbreg::ctrl() = usbreg::kUcResetSie;
    sysreg::pin_alternate() &= ~(sysreg::kPinUsbEnable | sysreg::kUdpPullUpEnable);

    // Give the host time to notice the disconnect before handing over.
    systick::delay_ms(10u);

    // Do not touch mstatus here. Writing it from this context traps as an
    // illegal instruction on the CH570Q QingKe core, and the application's
    // startup sets mstatus itself before entering main.
    using Entry = void (*)();
    reinterpret_cast<Entry>(kAppStart)();
    for (;;)
    {
    }
}

} // namespace ch572::usb
