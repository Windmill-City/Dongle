#pragma once

#include <cstddef>
#include <cstdint>

#include "ch572/compiler.hpp"
#include "ch572/platform/registers.hpp"
#include "ch572/serial/uart.hpp"
#include "ch572/util/ring_buffer.hpp"

namespace ch572::usb
{

// USB CDC ACM device exposing the transparent serial stream.
class UsbDevice
{
  public:
    UsbDevice() = default;

    void                 init();
    void                 poll();
    CH572_HIGH_CODE void irq_handler();

    // Attach the byte stream that feeds EP3 and owns the LineCoding. Keeping
    // this as a plain interface lets the USB side run with a test source
    // instead of the radio bridge.
    void                 set_source(serial::HostOutput source) noexcept;

    // Bytes arriving from the host, for a bridge to pull. main() wires this
    // into whichever source was attached above.
    serial::HostInput    host_input() noexcept;

    CH572_HIGH_CODE std::size_t read_host(void* dst, std::size_t max_length);

    // The LineCoding is owned by the attached source; the device only answers
    // the CDC class requests on its behalf.
    bool                        enumerated() const noexcept
    {
        return (usbreg::dev_addr() & kDeviceAddressMask) != 0;
    }

  private:
    static constexpr std::size_t  kEp0Size           = 64;
    static constexpr std::size_t  kPacketSize        = 64;
    static constexpr std::size_t  kUsbBufferSize     = 256;
    static constexpr std::uint8_t kInNak             = 2;
    static constexpr std::uint8_t kDeviceAddressMask = 0x7Fu;

    struct DeviceInfo
    {
        std::uint8_t config  = 0;
        std::uint8_t address = 0;
    };

    void                 endpoint_init();
    CH572_HIGH_CODE void handle_setup();
    void                 handle_ep0_in();

    // Load the next packet into EP3. Runs from the IN-complete interrupt and,
    // while the endpoint is not armed, from poll().
    CH572_HIGH_CODE void service_in();

    // Hardware IN state: MASK_UEP_T_RES is ACK only while a packet is loaded
    // and waiting for the host, and NAK once the buffer is free.
    static bool          ep3_armed() noexcept
    {
        return (usbreg::ep3_ctrl() & usbreg::kMaskUepTRes) == usbreg::kUepTResAck;
    }

    DeviceInfo          m_device_info;
    std::uint8_t        m_setup_request = 0;
    std::uint8_t        m_setup_type    = 0;
    std::uint8_t        m_setup_length  = 0;
    const std::uint8_t* m_descriptor    = nullptr;

    alignas(4) std::uint8_t m_ep0_buffer[kEp0Size]{};
    alignas(4) std::uint8_t m_ep1_buffer[8]{};
    alignas(4) std::uint8_t m_ep2_buffer[kPacketSize]{};
    alignas(4) std::uint8_t m_ep3_buffer[kPacketSize]{};

    RingBuffer<kUsbBufferSize> m_rx_buffer;
    serial::HostOutput          m_source;
};

} // namespace ch572::usb
