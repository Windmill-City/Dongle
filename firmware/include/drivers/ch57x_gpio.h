#ifndef __ch57x_GPIO_H__
#define __ch57x_GPIO_H__

#ifdef __cplusplus
extern "C" {
#endif

#define GPIO_Pin_0      (0x00000001)
#define GPIO_Pin_1      (0x00000002)
#define GPIO_Pin_2      (0x00000004)
#define GPIO_Pin_3      (0x00000008)
#define GPIO_Pin_4      (0x00000010)
#define GPIO_Pin_5      (0x00000020)
#define GPIO_Pin_6      (0x00000040)
#define GPIO_Pin_7      (0x00000080)
#define GPIO_Pin_8      (0x00000100)
#define GPIO_Pin_9      (0x00000200)
#define GPIO_Pin_10     (0x00000400)
#define GPIO_Pin_11     (0x00000800)
#define GPIO_Pin_12     (0x00001000)
#define GPIO_Pin_13     (0x00002000)
#define GPIO_Pin_14     (0x00004000)
#define GPIO_Pin_15     (0x00008000)
#define GPIO_Pin_16     (0x00010000)
#define GPIO_Pin_17     (0x00020000)
#define GPIO_Pin_18     (0x00040000)
#define GPIO_Pin_19     (0x00080000)
#define GPIO_Pin_20     (0x00100000)
#define GPIO_Pin_21     (0x00200000)
#define GPIO_Pin_22     (0x00400000)
#define GPIO_Pin_23     (0x00800000)
#define GPIO_Pin_All    (0xFFFFFFFF)

#define REMAP_RXD_PA2   0x00
#define REMAP_RXD_PA3   0x01
#define REMAP_RXD_PA0   0x02
#define REMAP_RXD_PA1   0x03
#define REMAP_RXD_PA4   0x04
#define REMAP_RXD_PA9   0x05
#define REMAP_RXD_PA10  0x06
#define REMAP_RXD_PA11  0x07

#define REMAP_TXD_PA3   0x00
#define REMAP_TXD_PA2   0x08
#define REMAP_TXD_PA1   0x10
#define REMAP_TXD_PA0   0x18
#define REMAP_TXD_PA7   0x20
#define REMAP_TXD_PA8   0x28
#define REMAP_TXD_PA11  0x30
#define REMAP_TXD_PA10  0x38

#define REMAP_TMR_DEFAULT   0x00
#define REMAP_TMR_MODE1     0x40
#define REMAP_TMR_MODE2     0x80
#define REMAP_TMR_MODE3     0xC0

#define REMAP_I2C_DEFAULT   0x00
#define REMAP_I2C_MODE1     0x200
#define REMAP_I2C_MODE2     0x400
#define REMAP_I2C_MODE3     0x600

typedef enum
{
    GPIO_ModeIN_Floating,
    GPIO_ModeIN_PU,
    GPIO_ModeIN_PD,
    GPIO_ModeOut_PP_5mA,
    GPIO_ModeOut_PP_20mA,

} GPIOModeTypeDef;

typedef enum
{
    GPIO_ITMode_LowLevel,
    GPIO_ITMode_HighLevel,
    GPIO_ITMode_FallEdge,
    GPIO_ITMode_RiseEdge,

} GPIOITModeTpDef;

void GPIOA_ModeCfg(uint32_t pin, GPIOModeTypeDef mode);

#define GPIOA_ResetBits(pin)      (R32_PA_CLR = pin)

#define GPIOA_SetBits(pin)        (R32_PA_SET = pin)

#define GPIOA_InverseBits(pin)    (R32_PA_OUT ^= pin)

#define GPIOA_ReadPort()          (R32_PA_PIN)

#define GPIOA_ReadPortPin(pin)    (R32_PA_PIN & (pin))

void GPIOA_ITModeCfg(uint32_t pin, GPIOITModeTpDef mode);

#define GPIOA_ReadITFlagPort()       (R16_PA_INT_IF)

#define GPIOA_ReadITFlagBit(pin)     (R16_PA_INT_IF & (pin))

#define GPIOA_ClearITFlagBit(pin)    (R16_PA_INT_IF = pin)

void GPIOPinRemap(FunctionalState s, uint16_t perph);

void GPIOADigitalCfg(FunctionalState s, uint16_t pin);

#ifdef __cplusplus
}
#endif

#endif
