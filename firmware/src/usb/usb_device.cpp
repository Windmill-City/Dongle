#include "ch572/usb/usb_device.hpp"

#include <cstring>

#include "ch572/config.hpp"
#include "ch572/platform/registers.hpp"
#include "ch572/usb/usb_common.hpp"

namespace ch572::usb {
namespace {

// CDC ACM class requests.
constexpr std::uint8_t kCdcSetLineCoding = 0x20;
constexpr std::uint8_t kCdcGetLineCoding = 0x21;
constexpr std::uint8_t kCdcSetControlLineState = 0x22;
constexpr std::uint8_t kCdcSendBreak = 0x23;

constexpr std::uint8_t kStall = 0xFF;
constexpr std::uint8_t kTokenOut = 0x00;
constexpr std::uint8_t kTokenIn = 0x20;
constexpr std::uint8_t kToggleOk = 0x40;

// The RF link runs at a fixed 2 Mbps, so the local CDC line settings are only
// a formality; report a fixed 115200 8N1 and accept any SET_LINE_CODING.
constexpr std::uint8_t kLineCoding[kCdcLineCodingSize] = {
    0x00, 0xC2, 0x01, 0x00, // 115200
    0x00,                   // one stop bit
    0x00,                   // no parity
    0x08,                   // eight data bits
};

} // namespace

UsbDevice &UsbDevice::instance()
{
    static UsbDevice device;
    return device;
}

void UsbDevice::init()
{
    sysreg::pin_alternate() &= ~sysreg::kPinDebugEnable;
    endpoint_init();

    // WCH implements the global interrupt enable in the custom CSR 0x800
    // (PFIC_EnableAllIRQ writes 0x800 = 0x88); mstatus alone does not gate
    // PFIC interrupts. Without this the USB interrupt never fires and the
    // host reports a device-descriptor failure.
    const std::uint32_t kGlobalIrqEnable = 0x88u;
    asm volatile("csrw 0x800, %0" : : "r"(kGlobalIrqEnable) : "memory");
}

void UsbDevice::endpoint_init()
{
    usbreg::ctrl() = 0;

    // EP1 is the CDC ACM notification endpoint and EP2 is the CDC data pair.
    usbreg::ep4_1_mod() = usbreg::kUep1TxEn;
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

void UsbDevice::handle_setup()
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
    m_setup_type = request_type;

    if (request_type == kRequestTypeStandard)
    {
        switch (m_setup_request)
        {
        case kReqGetDescriptor:
        {
            const std::uint8_t type = setup->value_h;
            const std::uint8_t *descriptor = find_descriptor(kDescriptors, type);
            std::uint8_t descriptor_length = 0;
            if (descriptor != nullptr)
            {
                descriptor_length = descriptor[0];
            }

            if (descriptor == nullptr)
            {
                length = kStall;
                break;
            }

            m_descriptor = descriptor;
            length = descriptor_length;
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
        switch (m_setup_request)
        {
        case kCdcSetLineCoding:
            length = data_direction == 0 ? 0 : kStall;
            break;

        case kCdcGetLineCoding:
            if (data_direction != 0)
            {
                length = static_cast<std::uint8_t>(kCdcLineCodingSize);
                std::memcpy(m_ep0_buffer, kLineCoding, kCdcLineCodingSize);
            }
            else
            {
                length = kStall;
            }
            break;

        case kCdcSetControlLineState:
        case kCdcSendBreak:
            length = data_direction == 0 ? 0 : kStall;
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

void UsbDevice::irq_handler()
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

            case kTokenIn | 1:
                usbreg::ep1_ctrl() ^= usbreg::kUepTTog;
                usbreg::ep1_ctrl() = (usbreg::ep1_ctrl() & 0xFCu) | kInNak;
                break;

            case kTokenOut | 0:
                if (toggle_ok)
                {
                    usbreg::ep0_ctrl() = usbreg::kUepRTog | usbreg::kUepTTog |
                                         usbreg::kUepRResNak | usbreg::kUepTResAck;
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
        // never starts.
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

void UsbDevice::handle_ep0_in()
{
    if (m_setup_type == kRequestTypeClass && m_setup_request == kCdcGetLineCoding)
    {
        usbreg::ep0_ctrl() = usbreg::kUepRTog | usbreg::kUepTTog |
                             usbreg::kUepRResAck | usbreg::kUepTResNak;
        return;
    }

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

    default:
        usbreg::ep0_t_len() = 0;
        usbreg::ep0_ctrl() = usbreg::kUepRTog | usbreg::kUepTTog |
                             usbreg::kUepRResNak | usbreg::kUepTResNak;
        break;
    }
}

void UsbDevice::poll()
{
    if (!enumerated())
    {
        return;
    }

    if ((usbreg::ep2_ctrl() & usbreg::kMaskUepTRes) == usbreg::kUepTResNak)
    {
        if (m_source != nullptr)
        {
            std::uint8_t length = static_cast<std::uint8_t>(kPacketSize);
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

std::size_t UsbDevice::read_host(void *dst, std::size_t max_length)
{
    if (m_rx_buffer.empty())
    {
        return 0;
    }

    const std::size_t length = m_rx_buffer.read(dst, max_length);
    if (m_rx_buffer.size() < (kUsbBufferSize - kPacketSize))
    {
        usbreg::ep2_ctrl() = (usbreg::ep2_ctrl() & ~usbreg::kMaskUepRRes) | usbreg::kUepRResAck;
    }
    return length;
}

void UsbDevice::set_source(ByteSource *source) noexcept
{
    m_source = source;
}

} // namespace ch572::usb

extern "C" {

CH572_INTERRUPT CH572_HIGH_CODE void USB_IRQHandler(void)
{
    ch572::usb::UsbDevice::instance().irq_handler();
}

}
