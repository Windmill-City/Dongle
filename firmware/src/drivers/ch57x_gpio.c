#include "ch57x_common.h"

void GPIOA_ModeCfg(uint32_t pin, GPIOModeTypeDef mode)
{
    switch(mode)
    {
        case GPIO_ModeIN_Floating:
            R32_PA_PD_DRV &= ~pin;
            R32_PA_PU &= ~pin;
            R32_PA_DIR &= ~pin;
            break;

        case GPIO_ModeIN_PU:
            R32_PA_PD_DRV &= ~pin;
            R32_PA_PU |= pin;
            R32_PA_DIR &= ~pin;
            break;

        case GPIO_ModeIN_PD:
            R32_PA_PD_DRV |= pin;
            R32_PA_PU &= ~pin;
            R32_PA_DIR &= ~pin;
            break;

        case GPIO_ModeOut_PP_5mA:
            R32_PA_PD_DRV &= ~pin;
            R32_PA_DIR |= pin;
            break;

        case GPIO_ModeOut_PP_20mA:
            R32_PA_PD_DRV |= pin;
            R32_PA_DIR |= pin;
            break;

        default:
            break;
    }
}

void GPIOA_ITModeCfg(uint32_t pin, GPIOITModeTpDef mode)
{
    switch(mode)
    {
        case GPIO_ITMode_LowLevel:
            R16_PA_INT_MODE &= ~pin;
            R32_PA_CLR |= pin;
            break;

        case GPIO_ITMode_HighLevel:
            R16_PA_INT_MODE &= ~pin;
            R32_PA_OUT |= pin;
            break;

        case GPIO_ITMode_FallEdge:
            R16_PA_INT_MODE |= pin;
            R32_PA_CLR |= pin;
            break;

        case GPIO_ITMode_RiseEdge:
            R16_PA_INT_MODE |= pin;
            R32_PA_OUT |= pin;
            break;

        default:
            break;
    }
    R16_PA_INT_IF = pin;
    R16_PA_INT_EN |= pin;
}

void GPIOPinRemap(FunctionalState s, uint16_t perph)
{
    if(s)
    {
        R16_PIN_ALTERNATE_H |= perph;
    }
    else
    {
        R16_PIN_ALTERNATE_H &= ~perph;
    }
}

void GPIOADigitalCfg(FunctionalState s, uint16_t pin)
{
    if(s)
    {
        R16_PIN_ALTERNATE &= ~pin;
    }
    else
    {
        R16_PIN_ALTERNATE |= pin;
    }
}
