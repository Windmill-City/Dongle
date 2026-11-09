#include "ch572/usb/ch341.hpp"

#include <cstring>

#include "ch572/config.hpp"
#include "ch572/platform/registers.hpp"
#include "ch572/usb/usb_common.hpp"

namespace ch572::usb {
namespace {

constexpr std::uint8_t kRequestDebugRead = 0x95;
constexpr std::uint8_t kRequestDebugWrite = 0x9A;
constexpr std::uint8_t kRequestUartInit = 0xA1;
constexpr std::uint8_t kRequestUartModemOut = 0xA4;
constexpr std::uint8_t kRequestGetVersion = 0x5F;

// Out-of-band IAP control. A vendor control request cannot collide with the
// transparent RF data stream, so no byte pattern in the payload can trigger a
// jump or a bootloader update.
constexpr std::uint8_t kRequestIapControl = 0xB1;
constexpr std::uint8_t kIapMarker = 0x5A;
constexpr std::uint8_t kIapTokenLow = 0x41;  // "AI"
constexpr std::uint8_t kIapTokenHigh = 0x49; // "I"
constexpr std::uint8_t kIapCommandJump = 0x01;
constexpr std::uint8_t kIapCommandBoot = 0x02;

constexpr std::uint8_t kClassSetLineCoding = 0x20;
constexpr std::uint8_t kClassGetLineCoding = 0x21;
constexpr std::uint8_t kClassSetControlLineState = 0x22;
constexpr std::uint8_t kClassSerialState = 0x20;

constexpr std::uint8_t kUart1Stop = 1;
constexpr std::uint8_t kUart2Stop = 2;
constexpr std::uint8_t kParityNone = 0x00;
constexpr std::uint8_t kParityOdd = 0x01;
constexpr std::uint8_t kParityEven = 0x02;
constexpr std::uint8_t kParityMark = 0x03;
constexpr std::uint8_t kParitySpace = 0x04;

constexpr std::uint8_t kStall = 0xFF;
constexpr std::uint8_t kTokenOut = 0x00;
constexpr std::uint8_t kTokenIn = 0x20;
constexpr std::uint8_t kToggleOk = 0x40;

} // namespace

Ch341Serial &Ch341Serial::instance()
{
    static Ch341Serial serial;
    return serial;
}

void Ch341Serial::init()
{
    sysreg::pin_alternate() &= ~sysreg::kPinDebugEnable;
    m_iap_request = false;
    m_iap_update_request = false;
    device_parameters_init();
    vendor_init();
    enable_irq(Irq::Usb);

    // WCH implements the global interrupt enable in the custom CSR 0x800
    // (PFIC_EnableAllIRQ writes 0x800 = 0x88); mstatus alone does not gate
    // PFIC interrupts. Without this the USB interrupt never fires, the setup
    // packet is only handled by the slow flash-resident poll loop, and the
    // host reports a device-descriptor failure.
    const std::uint32_t kGlobalIrqEnable = 0x88u;
    asm volatile("csrw 0x800, %0" : : "r"(kGlobalIrqEnable) : "memory");
}

void Ch341Serial::vendor_init()
{
    usbreg::ctrl() = 0;
    usbreg::ep4_1_mod() = usbreg::kUep1TxEn | usbreg::kUep1RxEn;
    usbreg::ep2_3_mod() = usbreg::kUep2RxEn | usbreg::kUep2TxEn;

    usbreg::ep0_dma() = static_cast<std::uint16_t>(reinterpret_cast<std::uintptr_t>(&m_ep0_buffer[0]));
    usbreg::ep1_dma() = static_cast<std::uint16_t>(reinterpret_cast<std::uintptr_t>(&m_ep1_buffer[0]));
    usbreg::ep2_dma() = static_cast<std::uint16_t>(reinterpret_cast<std::uintptr_t>(&m_ep2_buffer[0]));

    // EP0 must receive (ACK) so the host's first SETUP (GET_DESCRIPTOR) is
    // accepted. With RX=NAK the packet is rejected and enumeration never
    // starts.
    usbreg::ep0_ctrl() = usbreg::kUepRResAck | usbreg::kUepTResNak;
    usbreg::ep1_ctrl() = usbreg::kUepRResAck | usbreg::kUepTResNak;
    usbreg::ep2_ctrl() = usbreg::kUepRResAck | usbreg::kUepTResNak;

    usbreg::dev_addr() = 0;
    usbreg::dev_ctrl() = usbreg::kUdPdDis;
    usbreg::ctrl() = usbreg::kUcDevPuEn | usbreg::kUcIntBusy | usbreg::kUcDmaEn;
    sysreg::pin_alternate() |= sysreg::kPinUsbEnable | sysreg::kUdpPullUpEnable;
    usbreg::int_fg() = 0xFF;
    usbreg::int_en() = usbreg::kUieSuspend | usbreg::kUieTransfer | usbreg::kUieBusReset;

    enable_irq(Irq::Usb);

    usbreg::dev_ctrl() |= usbreg::kUdPortEn;

    m_device_info = DeviceInfo{};
}

void Ch341Serial::device_parameters_init()
{
    m_line_coding.baud_rate = 115200;
    m_line_coding.data_bits = 8;
    m_line_coding.parity = kParityNone;
    m_line_coding.stop_bits = kUart1Stop;
    m_line_coding.io_status = 0;
    m_line_coding_changed = true;

    for (std::size_t i = 0; i < kCh341RegisterCount; ++i)
    {
        m_ch341_register_address[i] = 0xFF;
        m_ch341_register_value[i] = 0x00;
    }
}

void Ch341Serial::handle_setup()
{
    auto *setup = reinterpret_cast<SetupRequest *>(m_ep0_buffer);
    std::uint8_t data_direction = kRequestTypeIn;

    m_setup_length = setup->length_l;
    if (setup->length_h != 0)
    {
        m_setup_length = 0xFF;
    }

    std::uint8_t length = 0;
    m_setup_request = setup->request;
    if ((setup->request_type & kRequestTypeIn) == 0)
    {
        data_direction = 0;
    }

    const std::uint8_t request_type = setup->request_type & kRequestTypeMask;

    if (request_type == kRequestTypeVendor)
    {
        switch (m_setup_request)
        {
        case kRequestDebugWrite:
        {
            const std::uint8_t address1 = m_ep0_buffer[2];
            const std::uint8_t address2 = m_ep0_buffer[3];
            const std::uint8_t value1 = m_ep0_buffer[4];
            const std::uint8_t value2 = m_ep0_buffer[5];

            if (address1 == 0x12 && address2 == 0x13)
            {
                m_line_coding.baud_rate = ch341_baud_rate(setup->index_l, setup->index_h);
                m_line_coding_changed = true;
            }
            else
            {
                ch341_register_write(address1, value1);
                ch341_register_write(address2, value2);
            }
            break;
        }

        case kRequestDebugRead:
        {
            std::uint8_t value1 = 0;
            std::uint8_t value2 = 0;
            ch341_register_read(setup->value_l, &value1);
            ch341_register_read(setup->value_h, &value2);
            length = 2;
            m_ep0_buffer[0] = value1;
            m_ep0_buffer[1] = value2;
            m_setup_length = length;
            break;
        }

        case kRequestUartInit:
        {
            if ((m_ep0_buffer[2] & 0x80u) != 0)
            {
                parse_line_coding(m_ep0_buffer[3]);
                std::uint8_t divider_high = setup->index_l;
                if ((divider_high & (1u << 6)) == 0)
                {
                    divider_high &= 0xC7u;
                }
                m_line_coding.baud_rate = ch341_baud_rate(divider_high, setup->index_h);
                m_line_coding_changed = true;
            }
            break;
        }

        case kRequestUartModemOut:
        {
            m_line_coding.io_status = m_ep0_buffer[2];
            m_line_coding_changed = true;
            break;
        }

        case kRequestGetVersion:
        {
            length = 2;
            m_ep0_buffer[0] = 0x30;
            m_ep0_buffer[1] = 0x00;
            m_setup_length = length;
            break;
        }

        case kRequestIapControl:
        {
            // bmRequestType = 0x40 (vendor, host-to-device, device).
            // wValue = 0x5A01 (jump to IAP) or 0x5A02 (bootloader update).
            // wIndex = 0x4941 acts as a token so stray vendor requests from a
            // generic driver cannot arm the device.
            const bool valid = data_direction == 0 && setup->length_l == 0 &&
                               setup->length_h == 0 &&
                               setup->value_h == kIapMarker &&
                               setup->index_l == kIapTokenLow &&
                               setup->index_h == kIapTokenHigh;
            if (valid && setup->value_l == kIapCommandJump)
            {
                m_iap_request = true;
            }
            else if (valid && setup->value_l == kIapCommandBoot)
            {
                m_iap_update_request = true;
            }
            break;
        }

        default:
            break;
        }
    }
    else if (request_type == kRequestTypeStandard)
    {
        switch (m_setup_request)
        {
        case kReqGetDescriptor:
        {
            const std::uint8_t *descriptor =
                find_descriptor(kDescriptors, setup->value_h, setup->value_l);
            if (descriptor == nullptr)
            {
                length = kStall;
                break;
            }

            // bLength is the first byte of every USB descriptor.
            m_descriptor = descriptor;
            length = descriptor[0];
            if (m_setup_length > length)
            {
                m_setup_length = length;
            }
            length = m_setup_length >= kEp0Size ? kEp0Size : m_setup_length;
            std::memcpy(m_ep0_buffer, m_descriptor, length);
            m_setup_length = static_cast<std::uint8_t>(m_setup_length - length);
            m_descriptor += length;
            break;
        }

        case kReqSetAddress:
            m_device_info.flags |= kAddressFlag;
            m_device_info.address = setup->value_l;
            break;

        case kReqGetConfiguration:
            m_ep0_buffer[0] = m_device_info.config;
            if (m_setup_length >= 1)
            {
                length = 1;
            }
            break;

        case kReqSetConfiguration:
            m_device_info.config = setup->value_l;
            break;

        case kReqClearFeature:
            if ((setup->request_type & kRecipientMask) == kRecipientDevice)
            {
                usbreg::ep1_ctrl() = (usbreg::ep1_ctrl() & ~(usbreg::kUepTTog | usbreg::kMaskUepTRes)) | usbreg::kUepTResNak;
                usbreg::ep2_ctrl() = (usbreg::ep2_ctrl() & ~(usbreg::kUepTTog | usbreg::kMaskUepTRes)) | usbreg::kUepTResNak;
            }
            else if ((setup->request_type & kRecipientMask) == kRecipientEndpoint)
            {
                switch (setup->index_l)
                {
                case 0x82:
                    usbreg::ep2_ctrl() = (usbreg::ep2_ctrl() & ~(usbreg::kUepTTog | usbreg::kMaskUepTRes)) | usbreg::kUepTResNak;
                    break;
                case 0x02:
                    usbreg::ep2_ctrl() = (usbreg::ep2_ctrl() & ~(usbreg::kUepRTog | usbreg::kMaskUepRRes)) | usbreg::kUepRResAck;
                    break;
                case 0x81:
                    usbreg::ep1_ctrl() = (usbreg::ep1_ctrl() & ~(usbreg::kUepTTog | usbreg::kMaskUepTRes)) | usbreg::kUepTResNak;
                    break;
                case 0x01:
                    usbreg::ep1_ctrl() = (usbreg::ep1_ctrl() & ~(usbreg::kUepRTog | usbreg::kMaskUepRRes)) | usbreg::kUepRResAck;
                    break;
                default:
                    length = kStall;
                    break;
                }
            }
            else
            {
                length = kStall;
            }
            break;

        case kReqGetInterface:
            m_ep0_buffer[0] = 0;
            if (m_setup_length >= 1)
            {
                length = 1;
            }
            break;

        case kReqGetStatus:
            m_ep0_buffer[0] = 0;
            m_ep0_buffer[1] = 0;
            length = m_setup_length >= 2 ? 2 : m_setup_length;
            break;

        default:
            length = kStall;
            break;
        }
    }
    else if (request_type == kRequestTypeClass)
    {
        if (data_direction == 0)
        {
            switch (m_setup_request)
            {
            case kClassSetLineCoding:
            case kClassSetControlLineState:
                length = 0;
                break;
            default:
                length = kStall;
                break;
            }
        }
        else
        {
            switch (m_setup_request)
            {
            case kClassGetLineCoding:
            {
                m_descriptor = m_ep0_buffer;
                length = static_cast<std::uint8_t>(sizeof(LineCoding));
                std::memcpy(m_ep0_buffer, &m_line_coding, sizeof(LineCoding));
                break;
            }
            case kClassSerialState:
                length = 2;
                m_ep0_buffer[0] = 0;
                m_ep0_buffer[1] = 0;
                break;
            default:
                length = kStall;
                break;
            }
        }
    }
    else
    {
        length = kStall;
    }

    if (length == kStall)
    {
        m_setup_request = kStall;
        usbreg::ep0_ctrl() = usbreg::kUepRTog | usbreg::kUepTTog |
                             usbreg::kUepRResStall | usbreg::kUepTResStall;
        return;
    }

    const bool data_in = (length <= kEp0Size);
    usbreg::ep0_t_len() = data_in ? length : 0;
    const std::uint8_t rx_response =
        (data_in && m_setup_request != kReqGetDescriptor) ? usbreg::kUepRResNak
                                                          : usbreg::kUepRResAck;
    usbreg::ep0_ctrl() = usbreg::kUepRTog | usbreg::kUepTTog |
                         rx_response | usbreg::kUepTResAck;
}

void Ch341Serial::irq_handler()
{
    if ((usbreg::int_fg() & usbreg::kUifTransfer) != 0)
    {
        if ((usbreg::int_st() & usbreg::kMaskUisToken) != usbreg::kMaskUisToken)
        {
            const std::uint8_t pid = usbreg::int_st();
            const std::uint8_t length = usbreg::rx_len();
            const bool toggle_ok = (usbreg::int_fg() & kToggleOk) != 0;

            switch (pid & 0x3Fu)
            {
            case kTokenOut | 2:
                if (toggle_ok)
                {
                    usbreg::ep2_ctrl() ^= usbreg::kUepRTog;
                    m_rx_buffer.write(m_ep2_buffer, length);

                    if (m_rx_buffer.size() > (kUsbBufferSize - kPacketSize))
                    {
                        usbreg::ep2_ctrl() = (usbreg::ep2_ctrl() & ~usbreg::kMaskUepRRes) | usbreg::kUepRResNak;
                    }
                }
                break;

            case kTokenIn | 2:
                usbreg::ep2_ctrl() ^= usbreg::kUepTTog;
                usbreg::ep2_ctrl() = (usbreg::ep2_ctrl() & ~usbreg::kMaskUepTRes) | kInNak;
                break;

            case kTokenOut | 1:
                if (toggle_ok)
                {
                    usbreg::ep1_ctrl() ^= usbreg::kUepRTog;
                    usbreg::ep1_ctrl() &= 0xF3u;
                }
                break;

            case kTokenIn | 1:
                usbreg::ep1_ctrl() ^= usbreg::kUepTTog;
                usbreg::ep1_ctrl() = (usbreg::ep1_ctrl() & 0xFCu) | kInNak;
                break;

            case kTokenOut | 0:
                if (toggle_ok)
                {
                    usbreg::ep0_ctrl() = (usbreg::ep0_ctrl() & 0xF3u) | 0x08u;
                    if (length != 0 && m_setup_request == kClassSetLineCoding)
                    {
                        std::memcpy(&m_line_coding.baud_rate, m_ep0_buffer, 4);
                        m_line_coding.stop_bits = m_ep0_buffer[4];
                        m_line_coding.parity = m_ep0_buffer[5];
                        m_line_coding.data_bits = m_ep0_buffer[6];
                        m_line_coding_changed = true;
                        usbreg::ep0_ctrl() = usbreg::kUepRTog | usbreg::kUepTTog |
                                             usbreg::kUepRResNak | usbreg::kUepTResAck;
                    }
                    else
                    {
                        usbreg::ep0_ctrl() = usbreg::kUepRTog | usbreg::kUepTTog |
                                             usbreg::kUepRResNak | usbreg::kUepTResNak;
                    }
                }
                break;

            case kTokenIn | 0:
                usbreg::ep0_ctrl() = (usbreg::ep0_ctrl() & 0xFCu) | kInNak;
                handle_ep0_in();
                break;

            default:
                break;
            }

            usbreg::int_fg() = usbreg::kUifTransfer;
        }

        if ((usbreg::int_st() & usbreg::kUisSetupAct) != 0)
        {
            // Handle the setup packet immediately in the ISR. Deferring it to
            // poll() (which runs from flash) delays the descriptor reply past
            // the host's enumeration timeout.
            handle_setup();
            usbreg::int_fg() = usbreg::kUifTransfer;
        }
    }
    else if ((usbreg::int_fg() & usbreg::kUifBusReset) != 0)
    {
        // After a bus reset EP0 must be ready to receive the host's SETUP
        // (ACK), otherwise the first GET_DESCRIPTOR is NAKed and enumeration
        // never starts. This matches the working IAP bootloader.
        usbreg::ep0_ctrl() = usbreg::kUepRResAck | usbreg::kUepTResNak;
        usbreg::ep1_ctrl() = usbreg::kUepRResAck | usbreg::kUepTResNak;
        usbreg::ep2_ctrl() = usbreg::kUepRResAck | usbreg::kUepTResNak;
        usbreg::dev_addr() = 0;
        m_device_info.address = 0;
        usbreg::int_fg() = usbreg::kUifBusReset;
    }
    else if ((usbreg::int_fg() & usbreg::kUifSuspend) != 0)
    {
        usbreg::int_fg() = usbreg::kUifSuspend;
    }
}

void Ch341Serial::handle_ep0_in()
{
    switch (m_setup_request)
    {
    case kReqGetDescriptor:
    {
        std::uint8_t length = m_setup_length >= kEp0Size ? kEp0Size : m_setup_length;
        std::memcpy(m_ep0_buffer, m_descriptor, length);
        m_setup_length = static_cast<std::uint8_t>(m_setup_length - length);
        m_descriptor += length;

        if (length != 0)
        {
            usbreg::ep0_t_len() = length;
            usbreg::ep0_ctrl() ^= usbreg::kUepTTog;
            usbreg::ep0_ctrl() &= ~0x03u;
        }
        else
        {
            usbreg::ep0_t_len() = length;
            usbreg::ep0_ctrl() = usbreg::kUepRTog | usbreg::kUepTTog |
                                 usbreg::kUepRResAck | usbreg::kUepTResNak;
        }
        break;
    }

    case kReqSetAddress:
        usbreg::dev_addr() = (usbreg::dev_addr() & 0x80u) | m_device_info.address;
        usbreg::ep0_ctrl() = usbreg::kUepRTog | usbreg::kUepTTog |
                             usbreg::kUepRResNak | usbreg::kUepTResNak;
        break;

    case kRequestDebugRead:
    case kRequestGetVersion:
    case kClassGetLineCoding:
        usbreg::ep0_ctrl() = usbreg::kUepRTog | usbreg::kUepTTog |
                             usbreg::kUepRResAck | usbreg::kUepTResNak;
        break;

    case kClassSetLineCoding:
        usbreg::ep0_ctrl() = usbreg::kUepRTog | usbreg::kUepTTog |
                             usbreg::kUepRResNak | usbreg::kUepTResNak;
        break;

    default:
        usbreg::ep0_t_len() = 0;
        usbreg::ep0_ctrl() = usbreg::kUepRTog | usbreg::kUepTTog |
                             usbreg::kUepRResNak | usbreg::kUepTResNak;
        break;
    }
}

void Ch341Serial::poll()
{
    if (!enumerated())
    {
        return;
    }

    if ((usbreg::ep2_ctrl() & usbreg::kMaskUepTRes) == usbreg::kUepTResNak)
    {
        if (m_source != nullptr)
        {
            std::uint8_t length = static_cast<std::uint8_t>(kPacketSize / 2);
            const std::size_t available =
                m_source->read(&m_ep2_buffer[kPacketSize], length);
            if (available != 0)
            {
                usbreg::ep2_t_len() = static_cast<std::uint8_t>(available);
                usbreg::ep2_ctrl() = (usbreg::ep2_ctrl() & ~usbreg::kMaskUepTRes) | usbreg::kUepTResAck;
            }
        }
    }
}

HostReadResult Ch341Serial::read_host(void *dst, std::size_t max_length)
{
    if (enumerated() && m_line_coding_changed)
    {
        m_line_coding_changed = false;
        return {HostReadStatus::line_coding_changed, 0};
    }

    if (!m_rx_buffer.empty())
    {
        const std::size_t length = m_rx_buffer.read(dst, max_length);
        if (m_rx_buffer.size() < (kUsbBufferSize - kPacketSize))
        {
            usbreg::ep2_ctrl() = (usbreg::ep2_ctrl() & ~usbreg::kMaskUepRRes) | usbreg::kUepRResAck;
        }
        return {HostReadStatus::data, length};
    }

    return {HostReadStatus::no_data, 0};
}

void Ch341Serial::set_source(ByteSource *source) noexcept
{
    m_source = source;
}

void Ch341Serial::parse_line_coding(std::uint8_t value)
{
    static constexpr std::uint8_t kDataBits[4] = {5, 6, 7, 8};

    std::uint8_t parity;
    switch (value & 0x38u)
    {
    case 0x00: parity = kParityNone; break;
    case 0x08: parity = kParityOdd; break;
    case 0x18: parity = kParityEven; break;
    case 0x28: parity = kParityMark; break;
    default: parity = kParitySpace; break;
    }

    m_line_coding.stop_bits = (value & 0x04u) != 0 ? kUart2Stop : kUart1Stop;
    m_line_coding.parity = parity;
    m_line_coding.data_bits = kDataBits[value & 0x03u];
}

void Ch341Serial::ch341_register_write(std::uint8_t address, std::uint8_t value)
{
    for (std::size_t i = 0; i < kCh341RegisterCount; ++i)
    {
        if (m_ch341_register_address[i] == address ||
            m_ch341_register_address[i] == 0xFF)
        {
            m_ch341_register_address[i] = address;
            m_ch341_register_value[i] = value;
            break;
        }
    }

    if (address == 0x18)
    {
        parse_line_coding(value);
    }
}

void Ch341Serial::ch341_register_read(std::uint8_t address, std::uint8_t *value) const
{
    *value = 0;
    for (std::size_t i = 0; i < kCh341RegisterCount; ++i)
    {
        if (m_ch341_register_address[i] == address)
        {
            *value = m_ch341_register_value[i];
            break;
        }
        if (m_ch341_register_address[i] == 0xFF)
        {
            break;
        }
    }

    if (address == 0x18)
    {
        *value = static_cast<std::uint8_t>(uartreg::lcr() | (*value & 0x40u));
    }
    else if (address == 0x06 || address == 0x07)
    {
        *value = 0;
    }
}

std::uint32_t Ch341Serial::ch341_baud_rate(std::uint8_t divider_high, std::uint8_t divider_low)
{
    if (divider_high == 0x87 && divider_low == 0xF3)
    {
        return 921600;
    }
    if (divider_high == 0x87 && divider_low == 0xD9)
    {
        return 307200;
    }

    const std::uint8_t divider = static_cast<std::uint8_t>(0u - divider_low);
    std::uint32_t reference_clock;

    switch (divider_high)
    {
    case 0x88:
    case 0xC8:
        reference_clock = 36923077u / 8u;
        break;
    case 0x89:
    case 0xC9:
        reference_clock = 36923077u / 8u / 256u;
        break;
    case 0x8A:
    case 0xCA:
        reference_clock = 32000000u / 8u;
        break;
    case 0x8B:
    case 0xCB:
        reference_clock = 32000000u / 8u / 256u;
        break;
    default:
        switch (divider_high & 0x7Fu)
        {
        case 3:
            reference_clock = 6000000;
            break;
        case 2:
            reference_clock = 750000;
            break;
        case 1:
            reference_clock = 93750;
            break;
        case 0:
            reference_clock = 11719;
            break;
        default:
            return 115200;
        }
        break;
    }

    if (divider == 0)
    {
        return 115200;
    }
    return reference_clock / divider;
}

} // namespace ch572::usb

extern "C" {

CH572_INTERRUPT CH572_HIGH_CODE void USB_IRQHandler(void)
{
    ch572::usb::Ch341Serial::instance().irq_handler();
}

}
