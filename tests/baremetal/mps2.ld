/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Memory map shared by the QEMU MPS2 AN385 (Cortex-M3), AN386 (Cortex-M4)
 * and AN500 (Cortex-M7) boards: 4 MiB ZBT SSRAM1 at 0 (used as flash) and
 * 4 MiB ZBT SSRAM2/3 at 0x20000000 for data, the RAM disk, heap and stack.
 */

ENTRY(Reset_Handler)

MEMORY
{
	FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 4M
	RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 4M
}

_estack = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
	.text :
	{
		KEEP(*(.isr_vector))
		*(.text*)
		*(.rodata*)
		KEEP(*(.init))
		KEEP(*(.fini))
		. = ALIGN(4);
	} > FLASH

	.ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } > FLASH
	.ARM.exidx :
	{
		__exidx_start = .;
		*(.ARM.exidx* .gnu.linkonce.armexidx.*)
		__exidx_end = .;
	} > FLASH

	.preinit_array : { PROVIDE_HIDDEN(__preinit_array_start = .); KEEP(*(.preinit_array*)); PROVIDE_HIDDEN(__preinit_array_end = .); } > FLASH
	.init_array : { PROVIDE_HIDDEN(__init_array_start = .); KEEP(*(SORT(.init_array.*))); KEEP(*(.init_array*)); PROVIDE_HIDDEN(__init_array_end = .); } > FLASH
	.fini_array : { PROVIDE_HIDDEN(__fini_array_start = .); KEEP(*(SORT(.fini_array.*))); KEEP(*(.fini_array*)); PROVIDE_HIDDEN(__fini_array_end = .); } > FLASH

	. = ALIGN(4);
	_sidata = .;

	.data : AT(_sidata)
	{
		. = ALIGN(4);
		_sdata = .;
		*(.data*)
		. = ALIGN(4);
		_edata = .;
	} > RAM

	.bss (NOLOAD) :
	{
		. = ALIGN(4);
		_sbss = .;
		*(.bss*)
		*(COMMON)
		. = ALIGN(4);
		_ebss = .;
	} > RAM

	/* newlib's _sbrk grows the heap from "end" towards the stack. */
	. = ALIGN(8);
	PROVIDE(end = .);
	PROVIDE(_end = .);
}
