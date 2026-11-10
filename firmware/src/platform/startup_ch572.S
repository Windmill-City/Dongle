/*
 * CH570/CH572 startup and vector table.
 *
 * The core starts at flash address 0, which jumps to the reset handler.  The
 * vector table is part of .highcode and is copied to RAM before mtvec is
 * programmed, so the WCH fast interrupt mode (mtvec = base | 3) can be used.
 */

    .section .init, "ax", @progbits
    .global _start
    .type _start, @function
    .balign 2
_start:
    j .Lhandle_reset
    .size _start, . - _start

    .section .vector, "ax", @progbits
    .balign 4
    .global _vector_base
    .type _vector_base, @object
_vector_base:
    .option push
    .option norvc

    .word 0
    .word 0
    .word NMI_Handler
    .word HardFault_Handler
    .word 0xF3F9BDA9
    .word Ecall_M_Mode_Handler
    .word 0
    .word 0
    .word Ecall_U_Mode_Handler
    .word Break_Point_Handler
    .word 0
    .word 0
    .word SysTick_Handler
    .word 0
    .word SWI_Handler
    .word 0

    .word 0
    .word GPIOA_IRQHandler
    .word 0
    .word SPI_IRQHandler
    .word BB_IRQHandler
    .word LLE_IRQHandler
    .word USB_IRQHandler
    .word 0
    .word TMR_IRQHandler
    .word 0
    .word 0
    .word UART_IRQHandler
    .word RTC_IRQHandler
    .word CMP_IRQHandler
    .word I2C_IRQHandler
    .word PWMX_IRQHandler
    .word 0
    .word KEYSCAN_IRQHandler
    .word ENCODER_IRQHandler
    .word WDOG_BAT_IRQHandler

    .option pop
    .size _vector_base, . - _vector_base

    .section .vector_handler, "ax", @progbits

    .weak NMI_Handler
    .weak HardFault_Handler
    .weak Ecall_M_Mode_Handler
    .weak Ecall_U_Mode_Handler
    .weak Break_Point_Handler
    .weak SysTick_Handler
    .weak SWI_Handler
    .weak GPIOA_IRQHandler
    .weak SPI_IRQHandler
    .weak BB_IRQHandler
    .weak LLE_IRQHandler
    .weak USB_IRQHandler
    .weak TMR_IRQHandler
    .weak UART_IRQHandler
    .weak RTC_IRQHandler
    .weak CMP_IRQHandler
    .weak I2C_IRQHandler
    .weak PWMX_IRQHandler
    .weak KEYSCAN_IRQHandler
    .weak ENCODER_IRQHandler
    .weak WDOG_BAT_IRQHandler

NMI_Handler:
HardFault_Handler:
Ecall_M_Mode_Handler:
Ecall_U_Mode_Handler:
Break_Point_Handler:
SysTick_Handler:
SWI_Handler:
GPIOA_IRQHandler:
SPI_IRQHandler:
BB_IRQHandler:
LLE_IRQHandler:
USB_IRQHandler:
TMR_IRQHandler:
UART_IRQHandler:
RTC_IRQHandler:
CMP_IRQHandler:
I2C_IRQHandler:
PWMX_IRQHandler:
KEYSCAN_IRQHandler:
ENCODER_IRQHandler:
WDOG_BAT_IRQHandler:
.Ldefault_handler:
    j .Ldefault_handler

    .section .handle_reset, "ax", @progbits
    .balign 4
.Lhandle_reset:
    .option push
    .option norelax
    la gp, __global_pointer$
    la sp, _eusrstack
    .option pop

    /* Copy .highcode from flash to RAM. */
    la a0, _highcode_lma
    la a1, _highcode_vma_start
    la a2, _highcode_vma_end
    bgeu a1, a2, .Lcopy_highcode_done
.Lcopy_highcode_loop:
    lw t0, 0(a0)
    sw t0, 0(a1)
    addi a0, a0, 4
    addi a1, a1, 4
    bltu a1, a2, .Lcopy_highcode_loop
.Lcopy_highcode_done:

    /* Copy .data from flash to RAM. */
    la a0, _data_lma
    la a1, _data_vma
    la a2, _edata
    bgeu a1, a2, .Lcopy_data_done
.Lcopy_data_loop:
    lw t0, 0(a0)
    sw t0, 0(a1)
    addi a0, a0, 4
    addi a1, a1, 4
    bltu a1, a2, .Lcopy_data_loop
.Lcopy_data_done:

    /* Clear .bss. */
    la a0, _sbss
    la a1, _ebss
    bgeu a0, a1, .Lclear_bss_done
.Lclear_bss_loop:
    sw zero, 0(a0)
    addi a0, a0, 4
    bltu a0, a1, .Lclear_bss_loop
.Lclear_bss_done:

    fence.i

    li t0, 0x25
    csrw 0xbc0, t0
    li t0, 0x3
    csrw 0x804, t0
    li t0, 0x1
    csrw 0xbc1, t0

    la t0, _vector_base
    ori t0, t0, 3
    csrw mtvec, t0

    /* MPP=machine (0x1800): main() runs in machine mode, which is required
       for the machine CSR writes in the platform layer. */
    li t0, 0x1880
    csrw mstatus, t0

    la t0, main
    csrw mepc, t0
    mret
