#ifndef __ch57x_GPIO_HPP__
#define __ch57x_GPIO_HPP__

#include <stdint.h>
#include "sfr.hpp"

inline constexpr uint32_t GPIO_Pin_0  = 0x00000001;
inline constexpr uint32_t GPIO_Pin_1  = 0x00000002;
inline constexpr uint32_t GPIO_Pin_2  = 0x00000004;
inline constexpr uint32_t GPIO_Pin_3  = 0x00000008;
inline constexpr uint32_t GPIO_Pin_4  = 0x00000010;
inline constexpr uint32_t GPIO_Pin_5  = 0x00000020;
inline constexpr uint32_t GPIO_Pin_6  = 0x00000040;
inline constexpr uint32_t GPIO_Pin_7  = 0x00000080;
inline constexpr uint32_t GPIO_Pin_8  = 0x00000100;
inline constexpr uint32_t GPIO_Pin_9  = 0x00000200;
inline constexpr uint32_t GPIO_Pin_10 = 0x00000400;
inline constexpr uint32_t GPIO_Pin_11 = 0x00000800;
inline constexpr uint32_t GPIO_Pin_12 = 0x00001000;
inline constexpr uint32_t GPIO_Pin_13 = 0x00002000;
inline constexpr uint32_t GPIO_Pin_14 = 0x00004000;
inline constexpr uint32_t GPIO_Pin_15 = 0x00008000;
inline constexpr uint32_t GPIO_Pin_16 = 0x00010000;
inline constexpr uint32_t GPIO_Pin_17 = 0x00020000;
inline constexpr uint32_t GPIO_Pin_18 = 0x00040000;
inline constexpr uint32_t GPIO_Pin_19 = 0x00080000;
inline constexpr uint32_t GPIO_Pin_20 = 0x00100000;
inline constexpr uint32_t GPIO_Pin_21 = 0x00200000;
inline constexpr uint32_t GPIO_Pin_22 = 0x00400000;
inline constexpr uint32_t GPIO_Pin_23 = 0x00800000;
inline constexpr uint32_t GPIO_Pin_All = 0xFFFFFFFF;

enum GPIOModeTypeDef : uint8_t
{
    GPIO_ModeIN_Floating,
    GPIO_ModeIN_PU,
    GPIO_ModeIN_PD,
    GPIO_ModeOut_PP_5mA,
    GPIO_ModeOut_PP_20mA,
};

void GPIOA_ModeCfg(uint32_t pin, GPIOModeTypeDef mode);

inline void GPIOA_ResetBits(uint32_t pin) { R32_PA_CLR = pin; }
inline void GPIOA_SetBits(uint32_t pin) { R32_PA_SET = pin; }

#endif
