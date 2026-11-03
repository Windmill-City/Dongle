	.section	.init,"ax",@progbits
	.global	_start
 	.global	_vector_base
	.align	1
_start:
	j	handle_reset

    .section    .vector,"ax",@progbits
    .align  1
_vector_base:
    .option norvc;

    .word   0
    .word   0
    .word   NMI_Handler
    .word   HardFault_Handler
    .word   0xF3F9BDA9
    .word   Ecall_M_Mode_Handler
    .word   0
    .word   0
    .word   Ecall_U_Mode_Handler
    .word   Break_Point_Handler
    .word   0
    .word   0
    .word   SysTick_Handler
    .word   0
    .word   SWI_Handler
    .word   0

    .word   0
    .word   GPIOA_IRQHandler
    .word   0
    .word   SPI_IRQHandler
    .word   BB_IRQHandler
    .word   LLE_IRQHandler
    .word   USB_IRQHandler
    .word   0
    .word   TMR_IRQHandler
    .word   0
    .word   0
    .word   UART_IRQHandler
    .word   RTC_IRQHandler
    .word   CMP_IRQHandler
    .word   I2C_IRQHandler
    .word   PWMX_IRQHandler
    .word   0
    .word   KEYSCAN_IRQHandler
    .word   ENCODER_IRQHandler
    .word   WDOG_BAT_IRQHandler

    .option rvc;

    .section    .vector_handler, "ax", @progbits
    .weak   NMI_Handler
    .weak   HardFault_Handler
    .weak   Ecall_M_Mode_Handler
    .weak   Ecall_U_Mode_Handler
    .weak   Break_Point_Handler
    .weak   SysTick_Handler
    .weak   SWI_Handler
    .weak   GPIOA_IRQHandler
    .weak   SPI_IRQHandler
    .weak   BB_IRQHandler
    .weak   LLE_IRQHandler
    .weak   USB_IRQHandler
    .weak   TMR_IRQHandler
    .weak   UART_IRQHandler
    .weak   RTC_IRQHandler
    .weak   CMP_IRQHandler
    .weak   I2C_IRQHandler
    .weak   PWMX_IRQHandler
    .weak   KEYSCAN_IRQHandler
    .weak   ENCODER_IRQHandler
    .weak   WDOG_BAT_IRQHandler

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
ENDOCE_IRQHandler:
WDOG_BAT_IRQHandler:
1:
	j 1b

	.section	.handle_reset,"ax",@progbits
	.weak	handle_reset
	.align	1
handle_reset:
.option push
.option	norelax
	la gp, __global_pointer$
1:
	la sp, _eusrstack
.option	pop

2:
    la a0, _highcode_lma
    la a1, _highcode_vma_start
    la a2, _highcode_vma_end
    bgeu a1, a2, 2f
1:
    lw t0, (a0)
    sw t0, (a1)
    addi a0, a0, 4
    addi a1, a1, 4
    bltu a1, a2, 1b

2:
	la a0, _data_lma
	la a1, _data_vma
	la a2, _edata
	bgeu a1, a2, 2f
1:
	lw t0, (a0)
	sw t0, (a1)
	addi a0, a0, 4
	addi a1, a1, 4
	bltu a1, a2, 1b
2:

	la a0, _sbss
	la a1, _ebss
	bgeu a0, a1, 2f
1:
	sw zero, (a0)
	addi a0, a0, 4
	bltu a0, a1, 1b
2:

	li t0, 0x25
	csrw 0xbc0, t0

	li t0, 0x3
	csrw 0x804, t0
	li t0, 0x1
	csrw 0xbc1, t0

    li t0, 0x88
    csrw mstatus, t0
	la t0, _vector_base

    ori t0, t0, 3
	csrw mtvec, t0

	la t0, main
	csrw mepc, t0

	mret
