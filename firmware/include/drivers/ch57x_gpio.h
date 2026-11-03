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

typedef enum
{
    GPIO_ModeIN_Floating,
    GPIO_ModeIN_PU,
    GPIO_ModeIN_PD,
    GPIO_ModeOut_PP_5mA,
    GPIO_ModeOut_PP_20mA,

} GPIOModeTypeDef;

void GPIOA_ModeCfg(uint32_t pin, GPIOModeTypeDef mode);

#define GPIOA_ResetBits(pin)      (R32_PA_CLR = pin)

#define GPIOA_SetBits(pin)        (R32_PA_SET = pin)

#ifdef __cplusplus
}
#endif

#endif
