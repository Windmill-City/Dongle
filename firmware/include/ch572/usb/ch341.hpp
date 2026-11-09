#pragma once

#include <cstddef>
#include <cstdint>

#include "ch572/config.hpp"
#include "ch572/usb/descriptors.hpp"
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

class Ch341Serial
{
public:
    static Ch341Serial &instance();

    void init();
    void poll();
    CH572_HIGH_CODE void irq_handler();
    void set_source(ByteSource *source) noexcept;

    CH572_HIGH_CODE HostReadResult read_host(void *dst, std::size_t max_length);
    const LineCoding &line_coding() const noexcept { return m_line_coding; }
    bool enumerated() const noexcept { return (usbreg_dev_addr() & 0x7Fu) != 0; }
    bool iap_requested() const noexcept { return m_iap_request; }

private:
    Ch341Serial() = default;

    static constexpr std::size_t kEp0Size = 64;
    static constexpr std::size_t kPacketSize = 64;
    static constexpr std::size_t kUsbBufferSize = 256;
    static constexpr std::size_t kCh341RegisterCount = 10;
    static constexpr std::size_t kIrqQueueDepth = 4;
    static constexpr std::uint8_t kInAck = 0;
    static constexpr std::uint8_t kInNak = 2;
    static constexpr std::uint8_t kAddressFlag = 0x04;

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

    struct DeviceInfo
    {
        std::uint8_t config = 0;
        std::uint8_t address = 0;
        std::uint8_t flags = 0;
    };

    void vendor_init();
    void device_parameters_init();
    CH572_HIGH_CODE void handle_setup(std::uint8_t length);
    void process_irq();
    CH572_HIGH_CODE void parse_line_coding(std::uint8_t value);
    CH572_HIGH_CODE void ch341_register_write(std::uint8_t address, std::uint8_t value);
    CH572_HIGH_CODE std::uint8_t ch341_register_read(std::uint8_t address, std::uint8_t *value);
    CH572_HIGH_CODE static std::uint32_t ch341_baud_rate(std::uint8_t divider_high, std::uint8_t divider_low);
    static void set_in_status(std::uint8_t endpoint, std::uint8_t status);

    static std::uint8_t usbreg_dev_addr() noexcept;

    DeviceInfo m_device_info;
    std::uint8_t m_setup_request = 0;
    std::uint8_t m_setup_length = 0;
    const std::uint8_t *m_descriptor = nullptr;

    alignas(4) std::uint8_t m_ep0_buffer[kEp0Size]{};
    alignas(4) std::uint8_t m_ep1_buffer[kPacketSize]{};
    alignas(4) std::uint8_t m_ep2_buffer[2 * kPacketSize]{};

    std::uint8_t m_ch341_register_address[kCh341RegisterCount]{};
    std::uint8_t m_ch341_register_value[kCh341RegisterCount]{};

    LineCoding m_line_coding{};
    bool m_line_coding_changed = false;

    std::uint8_t m_irq_write_index = 0;
    std::uint8_t m_irq_read_index = 0;
    volatile std::uint8_t m_irq_length[kIrqQueueDepth]{};
    volatile std::uint8_t m_irq_pid[kIrqQueueDepth]{};
    volatile std::uint8_t m_irq_pending[kIrqQueueDepth]{};

    RingBuffer<kUsbBufferSize> m_rx_buffer;
    ByteSource *m_source = nullptr;
    std::uint8_t m_iap_match = 0;
    volatile bool m_iap_request = false;
};

} // namespace ch572::usb

// The IRQ entry point is declared in the application translation unit.
