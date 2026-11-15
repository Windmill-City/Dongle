#pragma once

#include <cstddef>
#include <cstdint>

#include "ch572/compiler.hpp"
#include "ch572/serial/line_coding.hpp"

namespace ch572::serial
{

inline constexpr std::uint32_t kBaudRate     = 115200;
inline constexpr std::size_t   kRxBufferSize = 512;

struct HostInput
{
    void*       context = nullptr;
    std::size_t (*read)(void* context, void* dst, std::size_t max_length) = nullptr;
};

struct HostOutput
{
    void*       context = nullptr;
    std::size_t (*read)(void* context, void* dst, std::size_t max_length) = nullptr;
    void (*set_coding)(void* context, const LineCoding& coding) = nullptr;
    LineCoding (*coding)(void* context) = nullptr;
};

void                           init() noexcept;
void                           suspend() noexcept;
void                           resume() noexcept;
void                           configure(const LineCoding& coding) noexcept;

CH572_HIGH_CODE std::size_t read(std::uint8_t* dst, std::size_t max_length) noexcept;
void                        write(const std::uint8_t* src, std::size_t length) noexcept;

} // namespace ch572::serial
