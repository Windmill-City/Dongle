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

// HID class requests.
constexpr std::uint8_t kHidGetReport = 0x01;
constexpr std::uint8_t kHidGetIdle = 0x02;
constexpr std::uint8_t kHidGetProtocol = 0x03;
constexpr std::uint8_t kHidSetReport = 0x09;
constexpr std::uint8_t kHidSetIdle = 0x0A;
constexpr std::uint8_t kHidSetProtocol = 0x0B;

// Feature report ID 1: command, marker, "IAP", padding. The marker keeps a
// stray report from a generic HID tool from arming an upgrade.
constexpr std::uint8_t kHidMarker = 0x5A;
constexpr std::uint8_t kHidTokenI = 'I';
constexpr std::uint8_t kHidTokenA = 'A';
constexpr std::uint8_t kHidTokenP = 'P';
constexpr std::uint8_t kIapCommandAppUpdate = 0x01;
constexpr std::uint8_t kIapCommandBootUpdate = 0x02;
constexpr std::uint8_t kHidStatusOk = 0x00;
constexpr std::uint8_t kHidStatusInvalid = 0x01;
constexpr std::uint8_t kHidStatusUnknownCommand = 0x02;

constexpr std::uint8_t kStall = 0xFF;
constexpr std::uint8_t kTokenOut = 0x00;
constexpr std::uint8_t kTokenIn = 0x20;
constexpr std::uint8_t kToggleOk = 0x40;

} // namespace

UsbDevice &UsbDevice::instance()
{
    static UsbDevice device;
    return device;
}

void UsbDevice::init()
{
    sysreg::pin_alternate() &= ~sysreg::kPinDebugEnable;
    m_app_update_request = false;
    m_iap_update_request = false;
    m_hid_report_pending = false;
    m_hid_status = kHidStatusOk;
    m_hid_command = 0;
    device_parameters_init();
    endpoint_init();
    enable_irq(Irq::Usb);

    // WCH implements the global interrupt enable in the custom CSR 0x800
    // (PFIC_EnableAllIRQ writes 0x800 = 0x88); mstatus alone does not gate
    // PFIC interrupts. Without this the USB interrupt never fires, the setup
    // packet is only handled by the slow flash-resident poll loop, and the
    // host reports a device-descriptor failure.
    const std::uint32_t kGlobalIrqEnable = 0x88u;
    asm volatile("csrw 0x800, %0" : : "r"(kGlobalIrqEnable) : "memory");
}

void UsbDevice::endpoint_init()
{
    usbreg::ctrl() = 0;

    // EP1 is the CDC ACM notification endpoint, EP2 is the CDC data pair, and
    // EP3 is the HID interrupt IN endpoint.
    usbreg::ep4_1_mod() = usbreg::kUep1TxEn;
    usbreg::ep2_3_mod() = usbreg::kUep2RxEn | usbreg::kUep2TxEn |
                          usbreg::kUep3TxEn;

    usbreg::ep0_dma() = static_cast<std::uint16_t>(reinterpret_cast<std::uintptr_t>(&m_ep0_buffer[0]));
    usbreg::ep1_dma() = static_cast<std::uint16_t>(reinterpret_cast<std::uintptr_t>(&m_ep1_buffer[0]));
    usbreg::ep2_dma() = static_cast<std::uint16_t>(reinterpret_cast<std::uintptr_t>(&m_ep2_buffer[0]));
    usbreg::ep3_dma() = static_cast<std::uint16_t>(reinterpret_cast<std::uintptr_t>(&m_ep3_buffer[0]));

    // EP0 must receive (ACK) so the host's first SETUP (GET_DESCRIPTOR) is
    // accepted. With RX=NAK the packet is rejected and enumeration never
    // starts.
    usbreg::ep0_ctrl() = usbreg::kUepRResAck | usbreg::kUepTResNak;
    usbreg::ep1_ctrl() = usbreg::kUepRResAck | usbreg::kUepTResNak;
    usbreg::ep2_ctrl() = usbreg::kUepRResAck | usbreg::kUepTResNak;
    usbreg::ep3_ctrl() = usbreg::kUepRResNak | usbreg::kUepTResNak;

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

void UsbDevice::device_parameters_init()
{
    m_line_coding.baud_rate = 115200;
    m_line_coding.data_bits = 8;
    m_line_coding.parity = 0;
    m_line_coding.stop_bits = 1;
    m_line_coding.line_state = 0;
    m_line_coding_changed = true;
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
            const std::uint8_t *descriptor = nullptr;
            std::uint8_t descriptor_length = 0;

            if (type == kDescriptorHid)
            {
                descriptor = kHidDescriptor;
                descriptor_length = sizeof(kHidDescriptor);
            }
            else if (type == kDescriptorHidReport)
            {
                descriptor = kHidReportDescriptor;
                descriptor_length = sizeof(kHidReportDescriptor);
            }
            else
            {
                descriptor = find_descriptor(kDescriptors, type, setup->value_l);
                if (descriptor != nullptr)
                {
                    descriptor_length = descriptor[0];
                }
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
                usbreg::ep3_ctrl() = (usbreg::ep3_ctrl() & ~(usbreg::kUepTTog | usbreg::kMaskUepTRes)) | usbreg::kUepTResNak;
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
                case 0x83:
                    usbreg::ep3_ctrl() = (usbreg::ep3_ctrl() & ~(usbreg::kUepTTog | usbreg::kMaskUepTRes)) | usbreg::kUepTResNak;
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
        if (setup->index_l == kHidInterface)
        {
            switch (m_setup_request)
            {
            case kHidGetReport:
                if (data_direction != 0)
                {
                    length = static_cast<std::uint8_t>(kHidTransferSize);
                    m_ep0_buffer[0] = kHidStatusReportId;
                    m_ep0_buffer[1] = m_hid_status;
                    m_ep0_buffer[2] = m_hid_command;
                    for (std::size_t i = 3; i < kHidTransferSize; ++i)
                    {
                        m_ep0_buffer[i] = 0;
                    }
                }
                else
                {
                    length = kStall;
                }
                break;

            case kHidSetReport:
                length = data_direction == 0 ? 0 : kStall;
                break;

            case kHidGetIdle:
                if (data_direction != 0)
                {
                    m_ep0_buffer[0] = 0;
                    length = 1;
                }
                else
                {
                    length = kStall;
                }
                break;

            case kHidSetIdle:
            case kHidSetProtocol:
                length = data_direction == 0 ? 0 : kStall;
                break;

            case kHidGetProtocol:
                if (data_direction != 0)
                {
                    m_ep0_buffer[0] = 1;
                    length = 1;
                }
                else
                {
                    length = kStall;
                }
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
            case kCdcSetLineCoding:
                length = data_direction == 0 ? 0 : kStall;
                break;

            case kCdcGetLineCoding:
                if (data_direction != 0)
                {
                    length = static_cast<std::uint8_t>(kCdcLineCodingSize);
                    std::memcpy(m_ep0_buffer, &m_line_coding, kCdcLineCodingSize);
                }
                else
                {
                    length = kStall;
                }
                break;

            case kCdcSetControlLineState:
                if (data_direction == 0)
                {
                    m_line_coding.line_state = setup->value_l & 0x03u;
                    m_line_coding_changed = true;
                    length = 0;
                }
                else
                {
                    length = kStall;
                }
                break;

            case kCdcSendBreak:
                length = data_direction == 0 ? 0 : kStall;
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

void UsbDevice::handle_hid_report(std::uint8_t length)
{
    std::uint8_t status = kHidStatusInvalid;
    std::uint8_t command = 0;

    if (length >= 6 && m_ep0_buffer[0] == kHidReportId)
    {
        command = m_ep0_buffer[1];
        const bool valid =
            m_ep0_buffer[2] == kHidMarker &&
            m_ep0_buffer[3] == kHidTokenI &&
            m_ep0_buffer[4] == kHidTokenA &&
            m_ep0_buffer[5] == kHidTokenP;

        if (valid)
        {
            switch (command)
            {
            case kIapCommandAppUpdate:
                m_app_update_request = true;
                status = kHidStatusOk;
                break;
            case kIapCommandBootUpdate:
                m_iap_update_request = true;
                status = kHidStatusOk;
                break;
            default:
                status = kHidStatusUnknownCommand;
                break;
            }
        }
    }

    queue_hid_status(status, command);
}

void UsbDevice::queue_hid_status(std::uint8_t status, std::uint8_t command)
{
    m_hid_status = status;
    m_hid_command = command;
    m_ep3_buffer[0] = kHidStatusReportId;
    m_ep3_buffer[1] = status;
    m_ep3_buffer[2] = command;
    for (std::size_t i = 3; i < kHidTransferSize; ++i)
    {
        m_ep3_buffer[i] = 0;
    }
    m_hid_report_pending = true;
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

            case kTokenIn | 3:
                usbreg::ep3_ctrl() ^= usbreg::kUepTTog;
                usbreg::ep3_ctrl() = (usbreg::ep3_ctrl() & ~usbreg::kMaskUepTRes) | kInNak;
                break;

            case kTokenOut | 0:
                if (toggle_ok)
                {
                    usbreg::ep0_ctrl() = (usbreg::ep0_ctrl() & 0xF3u) | 0x08u;
                    if (length != 0 && m_setup_type == kRequestTypeClass &&
                        m_setup_request == kCdcSetLineCoding &&
                        length >= kCdcLineCodingSize)
                    {
                        std::memcpy(&m_line_coding.baud_rate, m_ep0_buffer, 4);
                        m_line_coding.stop_bits = m_ep0_buffer[4];
                        m_line_coding.parity = m_ep0_buffer[5];
                        m_line_coding.data_bits = m_ep0_buffer[6];
                        m_line_coding_changed = true;
                        usbreg::ep0_ctrl() = usbreg::kUepRTog | usbreg::kUepTTog |
                                             usbreg::kUepRResNak | usbreg::kUepTResAck;
                    }
                    else if (length != 0 && m_setup_type == kRequestTypeClass &&
                             m_setup_request == kHidSetReport)
                    {
                        handle_hid_report(length);
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
        // never starts.
        usbreg::ep0_ctrl() = usbreg::kUepRResAck | usbreg::kUepTResNak;
        usbreg::ep1_ctrl() = usbreg::kUepRResAck | usbreg::kUepTResNak;
        usbreg::ep2_ctrl() = usbreg::kUepRResAck | usbreg::kUepTResNak;
        usbreg::ep3_ctrl() = usbreg::kUepRResNak | usbreg::kUepTResNak;
        usbreg::dev_addr() = 0;
        m_device_info.address = 0;
        m_hid_report_pending = false;
        usbreg::int_fg() = usbreg::kUifBusReset;
    }
    else if ((usbreg::int_fg() & usbreg::kUifSuspend) != 0)
    {
        usbreg::int_fg() = usbreg::kUifSuspend;
    }
}

void UsbDevice::handle_ep0_in()
{
    if (m_setup_type == kRequestTypeClass)
    {
        switch (m_setup_request)
        {
        case kCdcGetLineCoding:
        case kHidGetReport:
        case kHidGetIdle:
        case kHidGetProtocol:
            usbreg::ep0_ctrl() = usbreg::kUepRTog | usbreg::kUepTTog |
                                 usbreg::kUepRResAck | usbreg::kUepTResNak;
            return;

        case kCdcSetLineCoding:
        case kCdcSetControlLineState:
        case kCdcSendBreak:
        case kHidSetReport:
        case kHidSetIdle:
        case kHidSetProtocol:
            usbreg::ep0_ctrl() = usbreg::kUepRTog | usbreg::kUepTTog |
                                 usbreg::kUepRResNak | usbreg::kUepTResNak;
            return;

        default:
            break;
        }
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

    if (m_hid_report_pending &&
        (usbreg::ep3_ctrl() & usbreg::kMaskUepTRes) == usbreg::kUepTResNak)
    {
        usbreg::ep3_t_len() = static_cast<std::uint8_t>(kHidTransferSize);
        usbreg::ep3_ctrl() = (usbreg::ep3_ctrl() & ~usbreg::kMaskUepTRes) |
                             usbreg::kUepTResAck;
        m_hid_report_pending = false;
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

HostReadResult UsbDevice::read_host(void *dst, std::size_t max_length)
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
