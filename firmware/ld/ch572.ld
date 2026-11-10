/*
 * CH570/CH572 memory layout for the directly booted application.
 *
 * The Makefile selects the flash window with --defsym:
 *   application: FLASH_ORIGIN=0x00000000, FLASH_LENGTH=0x0003C000
 */

ENTRY(_start)

MEMORY
{
    FLASH (rx)  : ORIGIN = DEFINED(FLASH_ORIGIN) ? FLASH_ORIGIN : 0x00000000,
                  LENGTH = DEFINED(FLASH_LENGTH) ? FLASH_LENGTH : 240K
    RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 12K
}

SECTIONS
{
    .start :
    {
        KEEP(*(.start))
        *(.vector_handler)
        . = ALIGN(4);
    } > FLASH AT> FLASH

    .highcode :
    {
        . = ALIGN(4);
        PROVIDE(_highcode_vma_start = .);
        . = ALIGN(1024);
        *(.vector)
        *(.highcode)
        *(.highcode.*)
        . = ALIGN(4);
        PROVIDE(_highcode_vma_end = .);
    } > RAM AT> FLASH

    PROVIDE(_highcode_lma = LOADADDR(.highcode));

    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH AT> FLASH

    PROVIDE(_data_lma = LOADADDR(.data));

    .data :
    {
        . = ALIGN(4);
        PROVIDE(_data_vma = .);
        *(.data .data.*)
        . = ALIGN(8);
        PROVIDE(__global_pointer$ = . + 0x800);
        *(.sdata .sdata.*)
        . = ALIGN(8);
        *(.srodata.cst16)
        *(.srodata.cst8)
        *(.srodata.cst4)
        *(.srodata.cst2)
        *(.srodata .srodata.*)
        . = ALIGN(4);
        PROVIDE(_edata = .);
    } > RAM AT> FLASH

    .bss :
    {
        . = ALIGN(4);
        PROVIDE(_sbss = .);
        *(.sbss*)
        *(.bss*)
        *(COMMON*)
        . = ALIGN(4);
        PROVIDE(_ebss = .);
    } > RAM AT> FLASH

    .stack ORIGIN(RAM) + LENGTH(RAM) :
    {
        . = ALIGN(4);
        PROVIDE(_eusrstack = .);
    } > RAM
}
