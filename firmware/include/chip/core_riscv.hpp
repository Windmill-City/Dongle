#ifndef __CORE_RISCV_HPP__
#define __CORE_RISCV_HPP__

#include <stdint.h>
#include <sfr.hpp>

#if defined ( __CC_ARM   )
#define __ASM            __asm
#define __INLINE         __inline

#elif defined ( __ICCARM__ )
#define __ASM           __asm
#define __INLINE        inline

#elif defined   (  __GNUC__  )
#define __ASM            __asm
#define __INLINE         inline

#elif defined   (  __TASKING__  )
#define __ASM            __asm
#define __INLINE         inline

#endif

#ifdef __cplusplus
#define __I                 volatile
#else
#define __I                 volatile const
#endif
#define __O                 volatile
#define __IO                volatile
#define RV_STATIC_INLINE    inline

enum FunctionalState : uint8_t {
    DISABLE = 0,
    ENABLE = !DISABLE
};
enum FlagStatus : uint8_t {
    RESET = 0,
    SET = !RESET
};
using ITStatus = FlagStatus;

struct PFIC_Type {
    __I uint32_t  ISR[8];
    __I uint32_t  IPR[8];
    __IO uint32_t ITHRESDR;
    uint8_t       RESERVED[8];
    __I uint32_t  GISR;
    __IO uint8_t  VTFIDR[4];
    uint8_t       RESERVED0[0x0C];
    __IO uint32_t VTFADDR[4];
    uint8_t       RESERVED1[0x90];
    __O uint32_t  IENR[8];
    uint8_t       RESERVED2[0x60];
    __O uint32_t  IRER[8];
    uint8_t       RESERVED3[0x60];
    __O uint32_t  IPSR[8];
    uint8_t       RESERVED4[0x60];
    __O uint32_t  IPRR[8];
    uint8_t       RESERVED5[0x60];
    __IO uint32_t IACTR[8];
    uint8_t       RESERVED6[0xE0];
    __IO uint8_t  IPRIOR[256];
    uint8_t       RESERVED7[0x810];
    __IO uint32_t SCTLR;
};

struct SysTick_Type {
    __IO uint32_t CTLR;
    __IO uint32_t SR;
    union
    {
        __IO uint32_t CNT;
        __IO uint32_t CNTL;
    };
    uint8_t       RESERVED[4];
    union
    {
        __IO uint32_t CMP;
        __IO uint32_t CMPL;
    };
    uint8_t       RESERVED0[4];
};

#define PFIC                    ((PFIC_Type *)0xE000E000)
#define SysTick                 ((SysTick_Type *)0xE000F000)

#define PFIC_KEY1               ((uint32_t)0xFA050000)
#define PFIC_KEY2               ((uint32_t)0xBCAF0000)
#define PFIC_KEY3               ((uint32_t)0xBEEF0000)

#define __nop()                 __asm__ volatile("nop")

#define read_csr(reg)           ({unsigned long __tmp;                      \
        __asm__ volatile ("csrr %0, " #reg : "=r"(__tmp));                  \
        __tmp; })

#define write_csr(reg, val)     ({                                          \
        if (__builtin_constant_p(val) && (unsigned long)(val) < 32)         \
        __asm__ volatile ("csrw  " #reg ", %0" :: "i"(val));                \
        else                                                                \
            __asm__ volatile ("csrw  " #reg ", %0" :: "r"(val)); })

__attribute__((always_inline)) RV_STATIC_INLINE uint32_t __risc_v_enable_irq(uint32_t mpie_mie)
{
    uint32_t result;

    __asm volatile("csrrs %0, 0x800, %1" : \
                   "=r"(result): "r"(mpie_mie) : "memory");
    return result;
}

__attribute__((always_inline)) RV_STATIC_INLINE uint32_t __risc_v_disable_irq(void)
{
    uint32_t result;

    __asm volatile("csrrc %0, 0x800, %1" : \
                   "=r"(result): "r"(0x88) : "memory");
    return result & 0x88;
}

#define PFIC_EnableAllIRQ()     {write_csr(0x800, 0x88);}
#define PFIC_DisableAllIRQ()    {write_csr(0x800, 0x80);asm volatile("fence.i");}

__attribute__((always_inline)) RV_STATIC_INLINE void PFIC_EnableIRQ(IRQn_Type IRQn)
{
    PFIC->IENR[((uint32_t)(IRQn) >> 5)] = (1 << ((uint32_t)(IRQn) & 0x1F));
}

__attribute__((always_inline)) RV_STATIC_INLINE void PFIC_DisableIRQ(IRQn_Type IRQn)
{
    PFIC->IRER[((uint32_t)(IRQn) >> 5)] = (1 << ((uint32_t)(IRQn) & 0x1F));
    asm volatile("fence.i");
}

__attribute__((always_inline)) RV_STATIC_INLINE uint32_t PFIC_GetStatusIRQ(IRQn_Type IRQn)
{
    return ((uint32_t)((PFIC->ISR[(uint32_t)(IRQn) >> 5] & (1 << ((uint32_t)(IRQn) & 0x1F))) ? 1 : 0));
}

__attribute__((always_inline)) RV_STATIC_INLINE uint32_t PFIC_GetPendingIRQ(IRQn_Type IRQn)
{
    return ((uint32_t)((PFIC->IPR[(uint32_t)(IRQn) >> 5] & (1 << ((uint32_t)(IRQn) & 0x1F))) ? 1 : 0));
}

__attribute__((always_inline)) RV_STATIC_INLINE void PFIC_SetPendingIRQ(IRQn_Type IRQn)
{
    PFIC->IPSR[((uint32_t)(IRQn) >> 5)] = (1 << ((uint32_t)(IRQn) & 0x1F));
}

__attribute__((always_inline)) RV_STATIC_INLINE void PFIC_ClearPendingIRQ(IRQn_Type IRQn)
{
    PFIC->IPRR[((uint32_t)(IRQn) >> 5)] = (1 << ((uint32_t)(IRQn) & 0x1F));
}

__attribute__((always_inline)) RV_STATIC_INLINE uint32_t PFIC_GetActive(IRQn_Type IRQn)
{
    return ((uint32_t)((PFIC->IACTR[(uint32_t)(IRQn) >> 5] & (1 << ((uint32_t)(IRQn) & 0x1F))) ? 1 : 0));
}

__attribute__((always_inline)) RV_STATIC_INLINE void PFIC_SetPriority(IRQn_Type IRQn, uint8_t priority)
{
    PFIC->IPRIOR[(uint32_t)(IRQn)] = priority ? 0x80 : 0;
}

__attribute__((always_inline)) RV_STATIC_INLINE void SetVTFIRQ(uint32_t addr, IRQn_Type IRQn, uint8_t num, FunctionalState NewState)
{
    if (num > 3)  return ;

    if (NewState != DISABLE)
    {
        PFIC->VTFIDR[num] = IRQn;
        PFIC->VTFADDR[num] = ((addr & 0xFFFFFFFE) | 0x1);
    }
    else
    {
        PFIC->VTFIDR[num] = IRQn;
        PFIC->VTFADDR[num] = ((addr & 0xFFFFFFFE) & (~0x1));
    }
}

__attribute__((always_inline)) RV_STATIC_INLINE void _SEV(void)
{

    PFIC->SCTLR |= (1 << 3) | (1 << 5);

}

__attribute__((always_inline)) RV_STATIC_INLINE void _WFE(void)
{
    PFIC->SCTLR |= (1 << 3);
    asm volatile("wfi");
}

__attribute__((always_inline)) RV_STATIC_INLINE void __WFE(void)
{
    _SEV();
    _WFE();
    _WFE();
}

__attribute__((always_inline)) RV_STATIC_INLINE void __WFI(void)
{
    PFIC->SCTLR &= ~(1 << 3);
    __asm__ volatile("wfi");
}

__attribute__((always_inline)) RV_STATIC_INLINE void PFIC_SystemReset(void)
{
    PFIC->SCTLR = 0x80000000;
}

__attribute__((always_inline)) RV_STATIC_INLINE uint32_t __get_MSTATUS(void)
{
    uint32_t result;

    __ASM volatile("csrr %0," "mstatus" : "=r"(result));
    return (result);
}

__attribute__((always_inline)) RV_STATIC_INLINE void __set_MSTATUS(uint32_t value)
{
    __ASM volatile("csrw mstatus, %0" : : "r"(value));
}

__attribute__((always_inline)) RV_STATIC_INLINE uint32_t __get_MISA(void)
{
    uint32_t result;

    __ASM volatile("csrr %0," "misa" : "=r"(result));
    return (result);
}

__attribute__((always_inline)) RV_STATIC_INLINE void __set_MISA(uint32_t value)
{
    __ASM volatile("csrw misa, %0" : : "r"(value));
}

__attribute__((always_inline)) RV_STATIC_INLINE uint32_t __get_MTVEC(void)
{
    uint32_t result;

    __ASM volatile("csrr %0," "mtvec" : "=r"(result));
    return (result);
}

__attribute__((always_inline)) RV_STATIC_INLINE void __set_MTVEC(uint32_t value)
{
    __ASM volatile("csrw mtvec, %0" : : "r"(value));
}

__attribute__((always_inline)) RV_STATIC_INLINE uint32_t __get_MSCRATCH(void)
{
    uint32_t result;

    __ASM volatile("csrr %0," "mscratch" : "=r"(result));
    return (result);
}

__attribute__((always_inline)) RV_STATIC_INLINE void __set_MSCRATCH(uint32_t value)
{
    __ASM volatile("csrw mscratch, %0" : : "r"(value));
}

__attribute__((always_inline)) RV_STATIC_INLINE uint32_t __get_MEPC(void)
{
    uint32_t result;

    __ASM volatile("csrr %0," "mepc" : "=r"(result));
    return (result);
}

__attribute__((always_inline)) RV_STATIC_INLINE void __set_MEPC(uint32_t value)
{
    __ASM volatile("csrw mepc, %0" : : "r"(value));
}

__attribute__((always_inline)) RV_STATIC_INLINE uint32_t __get_MCAUSE(void)
{
    uint32_t result;

    __ASM volatile("csrr %0," "mcause" : "=r"(result));
    return (result);
}

__attribute__((always_inline)) RV_STATIC_INLINE void __set_MCAUSE(uint32_t value)
{
    __ASM volatile("csrw mcause, %0" : : "r"(value));
}

__attribute__((always_inline)) RV_STATIC_INLINE uint32_t __get_MTVAL(void)
{
    uint32_t result;

    __ASM volatile("csrr %0," "mtval" : "=r"(result));
    return (result);
}

__attribute__((always_inline)) RV_STATIC_INLINE void __set_MTVAL(uint32_t value)
{
    __ASM volatile("csrw mtval, %0" : : "r"(value));
}

__attribute__((always_inline)) RV_STATIC_INLINE uint32_t __get_MVENDORID(void)
{
    uint32_t result;

    __ASM volatile("csrr %0," "mvendorid" : "=r"(result));
    return (result);
}

__attribute__((always_inline)) RV_STATIC_INLINE uint32_t __get_MARCHID(void)
{
    uint32_t result;

    __ASM volatile("csrr %0," "marchid" : "=r"(result));
    return (result);
}

__attribute__((always_inline)) RV_STATIC_INLINE uint32_t __get_MIMPID(void)
{
    uint32_t result;

    __ASM volatile("csrr %0," "mimpid" : "=r"(result));
    return (result);
}

__attribute__((always_inline)) RV_STATIC_INLINE uint32_t __get_MHARTID(void)
{
    uint32_t result;

    __ASM volatile("csrr %0," "mhartid" : "=r"(result));
    return (result);
}

__attribute__((always_inline)) RV_STATIC_INLINE uint32_t __get_SP(void)
{
    uint32_t result;

    __ASM volatile("mv %0," "sp" : "=r"(result) :);
    return (result);
}

__attribute__((always_inline)) RV_STATIC_INLINE void __MCPY(void *dst, void *start, void *end)
{
    __asm volatile("mcpy %2, %0, %1" : \
                   "+r"(start), "+r"(dst) : "r"(end) : "memory");
}

#define SysTick_SR_SWIE             (1 << 31)
#define SysTick_SR_CNTIF            (1 << 0)

#define SysTick_LOAD_RELOAD_Msk     (0xFFFFFFFF)
#define SysTick_CTLR_MODE           (1 << 4)
#define SysTick_CTLR_STRE           (1 << 3)
#define SysTick_CTLR_STCLK          (1 << 2)
#define SysTick_CTLR_STIE           (1 << 1)
#define SysTick_CTLR_STE            (1 << 0)

RV_STATIC_INLINE uint32_t SysTick_Config(uint32_t ticks)
{
    if ((ticks - 1) > SysTick_LOAD_RELOAD_Msk)
        return (1);

    SysTick->CNTL = 0;
    SysTick->CMP = ticks - 1;
    PFIC_EnableIRQ(SysTick_IRQn);
    SysTick->CTLR = SysTick_CTLR_STRE   |
                    SysTick_CTLR_STCLK  |
                    SysTick_CTLR_STIE   |
                    SysTick_CTLR_STE;
    return (0);
}

RV_STATIC_INLINE uint32_t __SysTick_Config(uint32_t ticks)
{
    if ((ticks - 1) > SysTick_LOAD_RELOAD_Msk)
        return (1);

    SysTick->CNTL = 0;
    SysTick->CMP = ticks - 1;
    SysTick->CTLR = SysTick_CTLR_STRE   |
                    SysTick_CTLR_STCLK  |
                    SysTick_CTLR_STIE   |
                    SysTick_CTLR_STE;
    return (0);
}

#endif
