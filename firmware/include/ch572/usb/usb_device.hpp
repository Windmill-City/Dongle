#pragma once

#include <cstddef>
#include <cstdint>

#include "ch572/config.hpp"
#include "ch572/platform/registers.hpp"
#include "ch572/usb/descriptors.hpp"
#include "ch572/usb/usb_common.hpp"
#include "ch572/util/ring_buffer.hpp"

namespace ch572::usb {

class ByteSource
{
public:
    virtual ~ByteSource() = default;
    virtual std::size_t read(std::uint8_t *dst, std::size_t max_length) = 0;
};

enum class HostReadStatus
{
    no_data,
    data,
    line_coding_changed,
};

struct HostReadResult
{
    HostReadStatus status;
    std::size_t length;
};

// USB composite device: CDC ACM for the transparent serial stream and a
// vendor-defined HID feature report that selects an upgrade target.
class UsbDevice
{
public:
    static UsbDevice &instance();

    void init();
    void poll();
    CH572_HIGH_CODE void irq_handler();
    void set_source(ByteSource *source) noexcept;

    CH572_HIGH_CODE HostReadResult read_host(void *dst, std::size_t max_length);
    const LineCoding &line_coding() const noexcept { return m_line_coding; }
    bool enumerated() const noexcept { return (usbreg::dev_addr() & 0x7Fu) != 0; }
    bool app_update_requested() const noexcept { return m_app_update_request; }
    bool iap_update_requested() const noexcept { return m_iap_update_request; }

private:
    UsbDevice() = default;

    static constexpr std::size_t kEp0Size = 64;
    static constexpr std::size_t kPacketSize = 64;
    static constexpr std::size_t kUsbBufferSize = 256;
    static constexpr std::uint8_t kInNak = 2;

    struct DeviceInfo
    {
        std::uint8_t config = 0;
        std::uint8_t address = 0;
    };

    void endpoint_init();
    void device_parameters_init();
    CH572_HIGH_CODE void handle_setup();
    void handle_ep0_in();
    CH572_HIGH_CODE void handle_hid_report(std::uint8_t length);
    CH572_HIGH_CODE void queue_hid_status(std::uint8_t status, std::uint8_t command);

    DeviceInfo m_device_info;
    std::uint8_t m_setup_request = 0;
    std::uint8_t m_setup_type = 0;
    std::uint8_t m_setup_length = 0;
    const std::uint8_t *m_descriptor = nullptr;

    alignas(4) std::uint8_t m_ep0_buffer[kEp0Size]{};
    alignas(4) std::uint8_t m_ep1_buffer[kPacketSize]{};
    alignas(4) std::uint8_t m_ep2_buffer[2 * kPacketSize]{};
    alignas(4) std::uint8_t m_ep3_buffer[kHidTransferSize]{};

    LineCoding m_line_coding{};
    volatile bool m_line_coding_changed = false;

    RingBuffer<kUsbBufferSize> m_rx_buffer;
    ByteSource *m_source = nullptr;
    volatile bool m_app_update_request = false;
    volatile bool m_iap_update_request = false;
    volatile bool m_hid_report_pending = false;
    std::uint8_t m_hid_status = 0;
    std::uint8_t m_hid_command = 0;
};

} // namespace ch572::usb
