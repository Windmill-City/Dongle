#include "CH57x_common.h"

void I2C_Init(I2C_ModeTypeDef I2C_Mode, uint32_t I2C_ClockSpeed, I2C_DutyTypeDef I2C_DutyCycle,
              I2C_AckTypeDef I2C_Ack, I2C_AckAddrTypeDef I2C_AckAddr, uint16_t I2C_OwnAddress1)
{
    uint32_t sysClock;
    uint16_t tmpreg;

    I2C_SoftwareResetCmd(ENABLE);
    I2C_SoftwareResetCmd(DISABLE);

    sysClock = GetSysClock();

    R16_I2C_CTRL2 &= ~RB_I2C_FREQ;
    R16_I2C_CTRL2 |= (sysClock / 1000000);

    R16_I2C_CTRL1 &= ~RB_I2C_PE;

    if(I2C_ClockSpeed <= 100000)
    {
        tmpreg = (sysClock / (I2C_ClockSpeed << 1)) & RB_I2C_CCR;

        if(tmpreg < 0x04)
            tmpreg = 0x04;

        R16_I2C_RTR = (((sysClock / 1000000) + 1) > 0x3F) ? 0x3F : ((sysClock / 1000000) + 1);
    }
    else
    {
        if(I2C_DutyCycle == I2C_DutyCycle_2)
        {
            tmpreg = (sysClock / (I2C_ClockSpeed * 3)) & RB_I2C_CCR;
        }
        else
        {
            tmpreg = (sysClock / (I2C_ClockSpeed * 25)) & RB_I2C_CCR;
            tmpreg |= I2C_DutyCycle_16_9;
        }

        if(tmpreg == 0)
        {
            tmpreg |= (uint16_t)0x0001;
        }

        tmpreg |= RB_I2C_F_S;
        R16_I2C_RTR = (uint16_t)((((sysClock / 1000000) * (uint16_t)300) / (uint16_t)1000) + (uint16_t)1);
    }
    R16_I2C_CKCFGR = tmpreg;

    R16_I2C_CTRL1 |= RB_I2C_PE;

    R16_I2C_CTRL1 &= ~(RB_I2C_SMBUS | RB_I2C_SMBTYPE | RB_I2C_ACK);
    R16_I2C_CTRL1 |= I2C_Mode | I2C_Ack;

    R16_I2C_OADDR1 &= 0;
    R16_I2C_OADDR1 |= I2C_AckAddr | I2C_OwnAddress1;
}

void I2C_Cmd(FunctionalState NewState)
{
    if(NewState != DISABLE)
        R16_I2C_CTRL1 |= RB_I2C_PE;
    else
        R16_I2C_CTRL1 &= ~RB_I2C_PE;
}

void I2C_GenerateSTART(FunctionalState NewState)
{
    if(NewState != DISABLE)
        R16_I2C_CTRL1 |= RB_I2C_START;
    else
        R16_I2C_CTRL1 &= ~RB_I2C_START;
}

void I2C_GenerateSTOP(FunctionalState NewState)
{
    if(NewState != DISABLE)
        R16_I2C_CTRL1 |= RB_I2C_STOP;
    else
        R16_I2C_CTRL1 &= ~RB_I2C_STOP;
}

void I2C_AcknowledgeConfig(FunctionalState NewState)
{
    if(NewState != DISABLE)
        R16_I2C_CTRL1 |= RB_I2C_ACK;
    else
        R16_I2C_CTRL1 &= ~RB_I2C_ACK;
}

void I2C_OwnAddress2Config(uint8_t Address)
{
    R16_I2C_OADDR2 &= ~RB_I2C_ADD2;
    R16_I2C_OADDR2 |= (uint16_t)(Address & RB_I2C_ADD2);
}

void I2C_DualAddressCmd(FunctionalState NewState)
{
    if(NewState != DISABLE)
        R16_I2C_OADDR2 |= RB_I2C_ENDUAL;
    else
        R16_I2C_OADDR2 &= ~RB_I2C_ENDUAL;
}

void I2C_GeneralCallCmd(FunctionalState NewState)
{
    if(NewState != DISABLE)
        R16_I2C_CTRL1 |= RB_I2C_ENGC;
    else
        R16_I2C_CTRL1 &= ~RB_I2C_ENGC;
}

void I2C_ITConfig(I2C_ITTypeDef I2C_IT, FunctionalState NewState)
{
    if(NewState != DISABLE)
        R16_I2C_CTRL2 |= I2C_IT;
    else
        R16_I2C_CTRL2 &= (uint16_t)~I2C_IT;
}

void I2C_SendData(uint8_t Data)
{
    R16_I2C_DATAR = Data;
}

uint8_t I2C_ReceiveData(void)
{
    return (uint8_t)R16_I2C_DATAR;
}

void I2C_Send7bitAddress(uint8_t Address, uint8_t I2C_Direction)
{
    if(I2C_Direction != I2C_Direction_Transmitter)
        Address |= OADDR1_ADD0_Set;
    else
        Address &= OADDR1_ADD0_Reset;

    R16_I2C_DATAR = Address;
}

void I2C_SoftwareResetCmd(FunctionalState NewState)
{
    if(NewState != DISABLE)
        R16_I2C_CTRL1 |= RB_I2C_SWRST;
    else
        R16_I2C_CTRL1 &= ~RB_I2C_SWRST;
}

void I2C_NACKPositionConfig(uint16_t I2C_NACKPosition)
{
    if(I2C_NACKPosition == I2C_NACKPosition_Next)
        R16_I2C_CTRL1 |= I2C_NACKPosition_Next;
    else
        R16_I2C_CTRL1 &= I2C_NACKPosition_Current;
}

void I2C_SMBusAlertConfig(uint16_t I2C_SMBusAlert)
{
    if(I2C_SMBusAlert == I2C_SMBusAlert_Low)
        R16_I2C_CTRL1 |= I2C_SMBusAlert_Low;
    else
        R16_I2C_CTRL1 &= I2C_SMBusAlert_High;
}

void I2C_TransmitPEC(FunctionalState NewState)
{
    if(NewState != DISABLE)
        R16_I2C_CTRL1 |= RB_I2C_PEC;
    else
        R16_I2C_CTRL1 &= ~RB_I2C_PEC;
}

void I2C_PECPositionConfig(uint16_t I2C_PECPosition)
{
    if(I2C_PECPosition == I2C_PECPosition_Next)
        R16_I2C_CTRL1 |= I2C_PECPosition_Next;
    else
        R16_I2C_CTRL1 &= I2C_PECPosition_Current;
}

void I2C_CalculatePEC(FunctionalState NewState)
{
    if(NewState != DISABLE)
        R16_I2C_CTRL1 |= RB_I2C_ENPEC;
    else
        R16_I2C_CTRL1 &= ~RB_I2C_ENPEC;
}

uint8_t I2C_GetPEC(void)
{
    return (R16_I2C_STAR2 >> 8);
}

void I2C_ARPCmd(FunctionalState NewState)
{
    if(NewState != DISABLE)
        R16_I2C_CTRL1 |= RB_I2C_EBARP;
    else
        R16_I2C_CTRL1 &= ~RB_I2C_EBARP;
}

void I2C_StretchClockCmd(FunctionalState NewState)
{
    if(NewState == DISABLE)
        R16_I2C_CTRL1 |= RB_I2C_NOSTRETCH;
    else
        R16_I2C_CTRL1 &= ~RB_I2C_NOSTRETCH;
}

void I2C_FastModeDutyCycleConfig(uint16_t I2C_DutyCycle)
{
    if(I2C_DutyCycle != I2C_DutyCycle_16_9)
        R16_I2C_CKCFGR &= ~I2C_DutyCycle_16_9;
    else
        R16_I2C_CKCFGR |= I2C_DutyCycle_16_9;
}

uint8_t I2C_CheckEvent(uint32_t I2C_EVENT)
{
    uint32_t lastevent = 0;
    uint32_t flag1 = 0, flag2 = 0;
    uint8_t  status = 0;

    flag1 = R16_I2C_STAR1;
    flag2 = R16_I2C_STAR2;
    flag2 = flag2 << 16;

    lastevent = (flag1 | flag2) & FLAG_Mask;

    if((lastevent & I2C_EVENT) == I2C_EVENT)
    {
        status = !0;
    }
    else
    {
        status = 0;
    }

    return status;
}

uint32_t I2C_GetLastEvent(void)
{
    uint32_t lastevent = 0;
    uint32_t flag1 = 0, flag2 = 0;

    flag1 = R16_I2C_STAR1;
    flag2 = R16_I2C_STAR2;
    flag2 = flag2 << 16;
    lastevent = (flag1 | flag2) & FLAG_Mask;

    return lastevent;
}

FlagStatus I2C_GetFlagStatus(uint32_t I2C_FLAG)
{
    FlagStatus    bitstatus = RESET;
    __IO uint32_t i2creg = 0, i2cxbase = 0;

    i2cxbase = (uint32_t)BA_I2C;
    i2creg = I2C_FLAG >> 28;
    I2C_FLAG &= FLAG_Mask;

    if(i2creg != 0)
    {
        i2cxbase += 0x14;
    }
    else
    {
        I2C_FLAG = (uint32_t)(I2C_FLAG >> 16);
        i2cxbase += 0x18;
    }

    if(((*(__IO uint32_t *)i2cxbase) & I2C_FLAG) != (uint32_t)RESET)
    {
        bitstatus = SET;
    }
    else
    {
        bitstatus = RESET;
    }

    return bitstatus;
}

void I2C_ClearFlag(uint32_t I2C_FLAG)
{
    uint32_t flagpos = 0;

    flagpos = I2C_FLAG & FLAG_Mask;
    R16_I2C_STAR1 = (uint16_t)~flagpos;
}

ITStatus I2C_GetITStatus(uint32_t I2C_IT)
{
    ITStatus bitstatus = RESET;
    uint32_t enablestatus = 0;

    enablestatus = (uint32_t)(((I2C_IT & ITEN_Mask) >> 16) & (R16_I2C_CTRL2));
    I2C_IT &= FLAG_Mask;

    if(((R16_I2C_STAR1 & I2C_IT) != (uint32_t)RESET) && enablestatus)
    {
        bitstatus = SET;
    }
    else
    {
        bitstatus = RESET;
    }

    return bitstatus;
}

void I2C_ClearITPendingBit(uint32_t I2C_IT)
{
    uint32_t flagpos = 0;

    flagpos = I2C_IT & FLAG_Mask;
    R16_I2C_STAR1 = (uint16_t)~flagpos;
}
