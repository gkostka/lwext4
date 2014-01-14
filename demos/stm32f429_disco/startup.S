/******************************************************************************
* author: Freddie Chopin, http://www.freddiechopin.info/
* file: startup.S
* last change: 2012-03-18
*
* chip: ARMv7-M(E) (Cortex-M3 / Cortex-M4)
* compiler: arm-none-eabi-gcc (GNU Tools for ARM Embedded Processors) 4.6.2
* 	20110921 (release) [ARM/embedded-4_6-branch revision 182083]
*
* description:
* ARMv7-M(E) (Cortex-M3 / Cortex-M4) assembly startup code
******************************************************************************/


#define PRIMASK_MASK_EXCEPTIONS_bit         0
#define PRIMASK_MASK_EXCEPTIONS             (1 << PRIMASK_MASK_EXCEPTIONS_bit)
#define FAULTMASK_MASK_EXCEPTIONS_bit       0
#define FAULTMASK_MASK_EXCEPTIONS           (1 << FAULTMASK_MASK_EXCEPTIONS_bit)
#define CONTROL_THREAD_UNPRIVILEGED_bit     0
#define CONTROL_ALTERNATE_STACK_bit         1
#define CONTROL_THREAD_UNPRIVILEGED         (1 << CONTROL_THREAD_UNPRIVILEGED_bit)
#define CONTROL_ALTERNATE_STACK             (1 << CONTROL_ALTERNATE_STACK_bit)


/*
+=============================================================================+
| ARMv7-M(E) (Cortex-M3 / Cortex-M4) startup code
+=============================================================================+
*/

.text
.balign 2
.syntax unified
.thumb
.thumb_func
.global Reset_Handler

Reset_Handler:

/*
+-----------------------------------------------------------------------------+
| Initialize the process stack pointer
+-----------------------------------------------------------------------------+
*/

	ldr		r0, =__process_stack_end
	msr		PSP, r0

/*
+-----------------------------------------------------------------------------+
| Thread mode uses process stack (PSP) and is privileged
+-----------------------------------------------------------------------------+
*/

	movs	r0, #CONTROL_ALTERNATE_STACK
	msr		CONTROL, r0
	isb

/*
+-----------------------------------------------------------------------------+
| Branch to low_level_init_0() function (.data and .bss are not initialized!)
+-----------------------------------------------------------------------------+
*/

	ldr		r0, =low_level_init_0
	blx		r0

/*
+-----------------------------------------------------------------------------+
| Initialize .data section
+-----------------------------------------------------------------------------+
*/

	ldr		r1, =__data_init_start
    ldr		r2, =__data_start
    ldr		r3, =__data_end

1:	cmp		r2, r3
	ittt	lo
	ldrlo	r0, [r1], #4
	strlo	r0, [r2], #4
	blo		1b

/*
+-----------------------------------------------------------------------------+
| Zero-init .bss section
+-----------------------------------------------------------------------------+
*/

	movs	r0, #0
	ldr		r1, =__bss_start
	ldr		r2, =__bss_end

1:	cmp		r1, r2
	itt		lo
	strlo	r0, [r1], #4
	blo		1b

/*
+-----------------------------------------------------------------------------+
| Call C++ constructors for global and static objects
+-----------------------------------------------------------------------------+
*/
#ifdef __USES_CXX
	ldr		r0, =__libc_init_array
	blx		r0
#endif

/*
+-----------------------------------------------------------------------------+
| Branch to low_level_init_1() function
+-----------------------------------------------------------------------------+
*/

	ldr		r0, =low_level_init_1
	blx		r0

/*
+-----------------------------------------------------------------------------+
| Branch to main() with link
+-----------------------------------------------------------------------------+
*/

	ldr		r0, =main
	blx		r0

/*
+-----------------------------------------------------------------------------+
| Call C++ destructors for global and static objects
+-----------------------------------------------------------------------------+
*/
#ifdef __USES_CXX
	ldr		r0, =__libc_fini_array
	blx		r0
#endif

/*
+-----------------------------------------------------------------------------+
| On return - loop till the end of the world
+-----------------------------------------------------------------------------+
*/

	b		.

/*
+=============================================================================+
| __default_low_level_init() - replacement for undefined low_level_init_0()
| and/or low_level_init_1(). This function just returns.
+=============================================================================+
*/

.text
.balign 2
.syntax unified
.thumb
.thumb_func
.global __default_low_level_init

__default_low_level_init:
	bx		lr

/*
+=============================================================================+
| assign undefined low_level_init_0() and/or low_level_init_1() to
| __default_low_level_init()
+=============================================================================+
*/

.weak	low_level_init_0
.global	low_level_init_0
.set	low_level_init_0, __default_low_level_init

.weak	low_level_init_1
.global	low_level_init_1
.set	low_level_init_1, __default_low_level_init

/******************************************************************************
* END OF FILE
******************************************************************************/
