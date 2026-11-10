#pragma once

#include <cstdint>

namespace ch572::usb {

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

// bmRequestType fields.
inline constexpr std::uint8_t kRequestTypeIn = 0x80;
inline constexpr std::uint8_t kRequestTypeMask = 0x60;
inline constexpr std::uint8_t kRequestTypeStandard = 0x00;
inline constexpr std::uint8_t kRequestTypeClass = 0x20;
inline constexpr std::uint8_t kRequestTypeVendor = 0x40;
inline constexpr std::uint8_t kRecipientMask = 0x1F;
inline constexpr std::uint8_t kRecipientDevice = 0x00;
inline constexpr std::uint8_t kRecipientEndpoint = 0x02;

// Standard requests shared by the application and the bootloader.
inline constexpr std::uint8_t kReqGetStatus = 0x00;
inline constexpr std::uint8_t kReqClearFeature = 0x01;
inline constexpr std::uint8_t kReqSetAddress = 0x05;
inline constexpr std::uint8_t kReqGetDescriptor = 0x06;
inline constexpr std::uint8_t kReqGetConfiguration = 0x08;
inline constexpr std::uint8_t kReqSetConfiguration = 0x09;
inline constexpr std::uint8_t kReqGetInterface = 0x0A;

inline constexpr std::uint8_t kDescriptorDevice = 1;
inline constexpr std::uint8_t kDescriptorConfig = 2;
inline constexpr std::uint8_t kDescriptorHid = 0x21;
inline constexpr std::uint8_t kDescriptorHidReport = 0x22;

// Descriptor tables differ between the application and the bootloader, but the
// lookup logic is the same.
struct DescriptorSet
{
    const std::uint8_t *device;
    const std::uint8_t *config;
};

inline const std::uint8_t *find_descriptor(const DescriptorSet &set,
                                           std::uint8_t type) noexcept
{
    switch (type)
    {
    case kDescriptorDevice:
        return set.device;
    case kDescriptorConfig:
        return set.config;
    default:
        return nullptr;
    }
}

} // namespace ch572::usb
