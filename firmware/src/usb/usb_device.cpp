#include "ch572/usb/usb_device.hpp"

#include <cstring>

#include "ch572/compiler.hpp"
#include "ch572/platform/registers.hpp"
#include "ch572/serial/line_coding.hpp"
#include "ch572/usb/descriptors.hpp"
#include "ch572/usb/usb_common.hpp"

namespace ch572::usb
{
namespace
{

// CDC ACM class requests.
constexpr std::uint8_t kCdcSetLineCoding  = 0x20;
constexpr std::uint8_t kCdcGetLineCoding  = 0x21;

constexpr std::uint8_t kStall             = 0xFF;
constexpr std::uint8_t kTokenEndpointMask = usbreg::kMaskUisToken | usbreg::kMaskUisEndpoint;

// The interrupt entry point is a plain C function with no context argument, so
// init() publishes the device the same way the radio bridge publishes its
// instance.
UsbDevice*             g_device           = nullptr;

// Used until a source supplies its own coding, so the CDC interface still
// answers enumeration and can be exercised without a bridge attached.
constexpr serial::LineCoding kDefaultCoding{115200u, serial::kStopBits1, serial::kParityNone, 8};

std::size_t            read_host_thunk(void* context, void* dst, std::size_t max_length) noexcept
{
    return static_cast<UsbDevice*>(context)->read_host(dst, max_length);
}

} // namespace

void UsbDevice::init()
{
    g_device = this;

    sysreg::pin_alternate() &= ~sysreg::kPinDebugEnable;
    endpoint_init();
}

void UsbDevice::endpoint_init()
{
    usbreg::ctrl()      = 0;

    // EP1 is the CDC ACM notification endpoint, EP2 receives the CDC data and
    // EP3 sends it. Separating the data endpoints gives TX and RX their own
    // 64-byte buffers instead of sharing the EP2 buffer pair.
    usbreg::ep4_1_mod() = usbreg::kUep1TxEn;
    usbreg::ep2_3_mod() = usbreg::kUep2RxEn | usbreg::kUep3TxEn;

    usbreg::ep0_dma()   = static_cast<std::uint16_t>(reinterpret_cast<std::uintptr_t>(&m_ep0_buffer[0]));
    usbreg::ep1_dma()   = static_cast<std::uint16_t>(reinterpret_cast<std::uintptr_t>(&m_ep1_buffer[0]));
    usbreg::ep2_dma()   = static_cast<std::uint16_t>(reinterpret_cast<std::uintptr_t>(&m_ep2_buffer[0]));
    usbreg::ep3_dma()   = static_cast<std::uint16_t>(reinterpret_cast<std::uintptr_t>(&m_ep3_buffer[0]));

    // EP0 must receive (ACK) so the host's first SETUP (GET_DESCRIPTOR) is
    // accepted. With RX=NAK the packet is rejected and enumeration never
    // starts.
    usbreg::ep0_ctrl()  = usbreg::kUepRResAck | usbreg::kUepTResNak;
    usbreg::ep1_ctrl()  = usbreg::kUepRResAck | usbreg::kUepTResNak | usbreg::kUepAutoTog;
    usbreg::ep2_ctrl()  = usbreg::kUepRResAck | usbreg::kUepTResNak | usbreg::kUepAutoTog;
    usbreg::ep3_ctrl()  = usbreg::kUepRResNak | usbreg::kUepTResNak | usbreg::kUepAutoTog;

    usbreg::dev_addr()  = 0;
    usbreg::dev_ctrl()  = usbreg::kUdPdDis;
    usbreg::ctrl()      = usbreg::kUcDevPuEn | usbreg::kUcIntBusy | usbreg::kUcDmaEn;
    sysreg::pin_alternate() |= sysreg::kPinUsbEnable | sysreg::kUdpPullUpEnable;
    usbreg::int_fg() = 0xFF;
    usbreg::int_en() = usbreg::kUieSuspend | usbreg::kUieTransfer | usbreg::kUieBusReset;

    enable_irq(Irq::Usb);

    usbreg::dev_ctrl() |= usbreg::kUdPortEn;

    m_device_info = DeviceInfo{};
}

void UsbDevice::handle_setup()
{
    auto*        setup          = reinterpret_cast<SetupRequest*>(m_ep0_buffer);
    std::uint8_t data_direction = kRequestTypeIn;

    m_setup_length              = setup->length_l;
    if (setup->length_h != 0)
    {
        m_setup_length = 0xFF;
    }

    std::uint8_t length = 0;
    m_setup_request     = setup->request;
    if ((setup->request_type & kRequestTypeIn) == 0)
    {
        data_direction = 0;
    }

    const std::uint8_t request_type = setup->request_type & kRequestTypeMask;
    m_setup_type                    = request_type;

    if (request_type == kRequestTypeStandard)
    {
        switch (m_setup_request)
        {
        case kReqGetDescriptor:
        {
            const std::uint8_t  type              = setup->value_h;
            const std::uint8_t* descriptor        = find_descriptor(kDescriptors, type);
            std::uint16_t       descriptor_length = 0;
            if (descriptor != nullptr)
            {
                descriptor_length = descriptor[0];
                if (type == kDescriptorConfig && descriptor[0] >= 4)
                {
                    // For a configuration descriptor bLength is only the 9-byte
                    // header; the full size is wTotalLength.
                    descriptor_length = static_cast<std::uint16_t>(descriptor[2] | (descriptor[3] << 8));
                }
                if (descriptor_length > 255)
                {
                    descriptor_length = 255;
                }
            }

            if (descriptor == nullptr)
            {
                length = kStall;
                break;
            }

            m_descriptor = descriptor;
            length       = static_cast<std::uint8_t>(descriptor_length);
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
            usbreg::ep1_ctrl()   = usbreg::kUepRResAck | usbreg::kUepTResNak | usbreg::kUepAutoTog;
            usbreg::ep2_ctrl()   = usbreg::kUepRResAck | usbreg::kUepTResNak | usbreg::kUepAutoTog;
            usbreg::ep3_ctrl()   = usbreg::kUepRResNak | usbreg::kUepTResNak | usbreg::kUepAutoTog;
            usbreg::ep1_t_len()  = 0;
            usbreg::ep2_t_len()  = 0;
            usbreg::ep3_t_len()  = 0;
            break;

        case kReqClearFeature:
            if ((setup->request_type & kRecipientMask) == kRecipientDevice)
            {
                usbreg::ep1_ctrl() =
                    (usbreg::ep1_ctrl() & ~(usbreg::kUepTTog | usbreg::kMaskUepTRes)) | usbreg::kUepTResNak;
                usbreg::ep2_ctrl() =
                    (usbreg::ep2_ctrl() & ~(usbreg::kUepTTog | usbreg::kMaskUepTRes)) | usbreg::kUepTResNak;
                usbreg::ep3_ctrl() =
                    (usbreg::ep3_ctrl() & ~(usbreg::kUepTTog | usbreg::kMaskUepTRes)) | usbreg::kUepTResNak;
            }
            else if ((setup->request_type & kRecipientMask) == kRecipientEndpoint)
            {
                switch (setup->index_l)
                {
                case 0x83:
                    usbreg::ep3_ctrl() =
                        (usbreg::ep3_ctrl() & ~(usbreg::kUepTTog | usbreg::kMaskUepTRes)) | usbreg::kUepTResNak;
                    break;
                case 0x02:
                    usbreg::ep2_ctrl() =
                        (usbreg::ep2_ctrl() & ~(usbreg::kUepRTog | usbreg::kMaskUepRRes)) | usbreg::kUepRResAck;
                    break;
                case 0x81:
                    usbreg::ep1_ctrl() =
                        (usbreg::ep1_ctrl() & ~(usbreg::kUepTTog | usbreg::kMaskUepTRes)) | usbreg::kUepTResNak;
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
            length          = m_setup_length >= 2 ? 2 : m_setup_length;
            break;

        default:
            length = kStall;
            break;
        }
    }
    else if (request_type == kRequestTypeClass)
    {
        if (data_direction != 0 && m_setup_request == kCdcGetLineCoding)
        {
            serial::LineCoding coding = kDefaultCoding;
            if (m_source.coding != nullptr)
            {
                coding = m_source.coding(m_source.context);
            }
            length = static_cast<std::uint8_t>(kCdcLineCodingSize);
            std::memcpy(m_ep0_buffer, &coding, kCdcLineCodingSize);
        }
        else
        {
            length = 0;
        }
    }
    else
    {
        length = kStall;
    }

    if (length == kStall)
    {
        m_setup_request    = kStall;
        usbreg::ep0_ctrl() = usbreg::kUepRTog | usbreg::kUepTTog | usbreg::kUepRResStall | usbreg::kUepTResStall;
        return;
    }

    const bool data_in  = (length <= kEp0Size);
    const bool out_data = (data_direction == 0 && m_setup_length != 0);
    usbreg::ep0_t_len() = data_in ? length : 0;
    const std::uint8_t rx_response =
        out_data ? usbreg::kUepRResAck
                 : ((data_in && m_setup_request != kReqGetDescriptor) ? usbreg::kUepRResNak : usbreg::kUepRResAck);
    usbreg::ep0_ctrl() = usbreg::kUepRTog | usbreg::kUepTTog | rx_response | usbreg::kUepTResAck;
}

void UsbDevice::irq_handler()
{
    if ((usbreg::int_fg() & usbreg::kUifTransfer) != 0)
    {
        if ((usbreg::int_st() & usbreg::kMaskUisToken) != usbreg::kMaskUisToken)
        {
            const std::uint8_t pid      = usbreg::int_st();
            const std::uint8_t length   = usbreg::rx_len();
            const std::uint8_t flags    = usbreg::int_fg();
            const bool         accepted = (flags & usbreg::kUifToggleOk) != 0 && (flags & usbreg::kUifNak) == 0;

            switch (pid & kTokenEndpointMask)
            {
            case usbreg::kUisTokenIn | 0:
                handle_ep0_in();
                break;

            case usbreg::kUisTokenIn | 1:
                usbreg::ep1_ctrl() = (usbreg::ep1_ctrl() & ~usbreg::kMaskUepTRes) | usbreg::kUepTResNak;
                break;

            case usbreg::kUisTokenIn | 3:
                service_in();
                break;

            case usbreg::kUisTokenOut | 0:
                if (accepted)
                {
                    if (m_setup_type == kRequestTypeClass && m_setup_request == kCdcSetLineCoding
                        && static_cast<std::size_t>(length) == sizeof(serial::LineCoding))
                    {
                        serial::LineCoding coding;
                        std::memcpy(&coding, m_ep0_buffer, sizeof(coding));
                        if (m_source.set_coding != nullptr)
                        {
                            m_source.set_coding(m_source.context, coding);
                        }
                    }

                    usbreg::ep0_ctrl() =
                        usbreg::kUepRTog | usbreg::kUepTTog | usbreg::kUepRResNak | usbreg::kUepTResAck;
                }
                break;

            case usbreg::kUisTokenOut | 2:
                if (accepted)
                {
                    m_rx_buffer.write(m_ep2_buffer, length);

                    if (m_rx_buffer.size() > (kUsbBufferSize - kPacketSize))
                    {
                        usbreg::ep2_ctrl() = (usbreg::ep2_ctrl() & ~usbreg::kMaskUepRRes) | usbreg::kUepRResNak;
                    }
                }
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
        usbreg::ep0_ctrl()    = usbreg::kUepRResAck | usbreg::kUepTResNak;
        usbreg::ep1_ctrl()    = usbreg::kUepRResAck | usbreg::kUepTResNak | usbreg::kUepAutoTog;
        usbreg::ep2_ctrl()    = usbreg::kUepRResAck | usbreg::kUepTResNak | usbreg::kUepAutoTog;
        usbreg::ep3_ctrl()    = usbreg::kUepRResNak | usbreg::kUepTResNak | usbreg::kUepAutoTog;
        usbreg::dev_addr()    = 0;
        m_device_info.address = 0;
        usbreg::int_fg()      = usbreg::kUifBusReset;
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
        usbreg::ep0_ctrl() = usbreg::kUepRTog | usbreg::kUepTTog | usbreg::kUepRResAck | usbreg::kUepTResNak;
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
            usbreg::ep0_ctrl() = (usbreg::ep0_ctrl() & ~usbreg::kMaskUepTRes) | usbreg::kUepTResAck;
        }
        else
        {
            usbreg::ep0_t_len() = length;
            usbreg::ep0_ctrl()  = usbreg::kUepRTog | usbreg::kUepTTog | usbreg::kUepRResAck | usbreg::kUepTResNak;
        }
        break;
    }

    case kReqSetAddress:
        usbreg::dev_addr() = (usbreg::dev_addr() & ~kDeviceAddressMask) | m_device_info.address;
        usbreg::ep0_ctrl() = usbreg::kUepRTog | usbreg::kUepTTog | usbreg::kUepRResNak | usbreg::kUepTResNak;
        break;

    default:
        usbreg::ep0_t_len() = 0;
        usbreg::ep0_ctrl()  = usbreg::kUepRTog | usbreg::kUepTTog | usbreg::kUepRResNak | usbreg::kUepTResNak;
        break;
    }
}

void UsbDevice::poll()
{
    if (!enumerated())
    {
        return;
    }

    CriticalSection guard;
    if (!ep3_armed())
    {
        service_in();
    }
}

void UsbDevice::service_in()
{
    std::size_t available = 0;
    if (m_source.read != nullptr)
    {
        available = m_source.read(m_source.context, m_ep3_buffer, sizeof(m_ep3_buffer));
    }

    if (available != 0)
    {
        usbreg::ep3_t_len() = static_cast<std::uint8_t>(available);
        usbreg::ep3_ctrl()  = (usbreg::ep3_ctrl() & ~usbreg::kMaskUepTRes) | usbreg::kUepTResAck;
    }
    else
    {
        usbreg::ep3_ctrl() = (usbreg::ep3_ctrl() & ~usbreg::kMaskUepTRes) | kInNak;
    }
}

std::size_t UsbDevice::read_host(void* dst, std::size_t max_length)
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

void UsbDevice::set_source(serial::HostOutput source) noexcept
{
    m_source = source;
}

serial::HostInput UsbDevice::host_input() noexcept
{
    return serial::HostInput{this, &read_host_thunk};
}

} // namespace ch572::usb

extern "C"
{
CH572_INTERRUPT CH572_HIGH_CODE void USB_IRQHandler(void)
{
    if (ch572::usb::g_device != nullptr)
    {
        ch572::usb::g_device->irq_handler();
    }
}
}
