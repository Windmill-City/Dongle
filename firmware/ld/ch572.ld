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
    .init :
    {
        _sinit = .;
        . = ALIGN(4);
        KEEP(*(SORT_NONE(.init)))
        . = ALIGN(4);
        _einit = .;
    } > FLASH AT> FLASH

    .highcode_initlalign :
    {
        . = ALIGN(4);
        PROVIDE(_highcode_init_lma = .);
    } > FLASH AT> FLASH

    .highcodelalign :
    {
        . = ALIGN(4);
        PROVIDE(_highcode_lma = .);
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

    .text :
    {
        . = ALIGN(4);
        KEEP(*(SORT_NONE(.handle_reset)))
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata*)
        *(.sdata2.*)
        *(.glue_7)
        *(.glue_7t)
        *(.gnu.linkonce.t.*)
        . = ALIGN(4);
    } > FLASH AT> FLASH

    .fini :
    {
        KEEP(*(SORT_NONE(.fini)))
        . = ALIGN(4);
    } > FLASH AT> FLASH

    PROVIDE(_etext = .);
    PROVIDE(_eitcm = .);

    .dlalign :
    {
        . = ALIGN(4);
        PROVIDE(_data_lma = .);
    } > FLASH AT> FLASH

    .data :
    {
        . = ALIGN(4);
        PROVIDE(_data_vma = .);
        *(.gnu.linkonce.r.*)
        *(.data .data.*)
        *(.gnu.linkonce.d.*)
        . = ALIGN(8);
        PROVIDE(__global_pointer$ = . + 0x800);
        *(.sdata .sdata.*)
        *(.gnu.linkonce.s.*)
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
        *(.gnu.linkonce.sb.*)
        *(.bss*)
        *(.gnu.linkonce.b.*)
        *(COMMON*)
        . = ALIGN(4);
        PROVIDE(_ebss = .);
    } > RAM AT> FLASH

    PROVIDE(_end = _ebss);
    PROVIDE(end = .);

    .stack ORIGIN(RAM) + LENGTH(RAM) :
    {
        . = ALIGN(4);
        PROVIDE(_eusrstack = .);
    } > RAM
}
