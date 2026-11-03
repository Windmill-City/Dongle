#pragma once

#include <cstddef>
#include <cstdint>

namespace ch572::usb {

struct __attribute__((packed)) LineCoding
{
    std::uint32_t baud_rate;
    std::uint8_t stop_bits;
    std::uint8_t parity;
    std::uint8_t data_bits;
    std::uint8_t io_status;
};

static_assert(sizeof(LineCoding) == 8, "CH341 line coding is eight bytes");

inline constexpr std::uint16_t kVendorId = 0x1A86;
inline constexpr std::uint16_t kProductId = 0x7523;

extern const std::uint8_t kDeviceDescriptor[];
extern const std::size_t kDeviceDescriptorSize;
extern const std::uint8_t kConfigDescriptor[];
extern const std::size_t kConfigDescriptorSize;
extern const std::uint8_t kQualifierDescriptor[];
extern const std::size_t kQualifierDescriptorSize;
extern const std::uint8_t kLanguageDescriptor[];
extern const std::size_t kLanguageDescriptorSize;
extern const std::uint8_t kManufacturerDescriptor[];
extern const std::size_t kManufacturerDescriptorSize;
extern const std::uint8_t kProductDescriptor[];
extern const std::size_t kProductDescriptorSize;
extern const std::uint8_t kSerialDescriptor[];
extern const std::size_t kSerialDescriptorSize;

} // namespace ch572::usb
