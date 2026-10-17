/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Minimal Cortex-M startup for the bare-metal test: vector table,
 * .data/.bss initialisation, FPU enable and semihosting console/exit.
 */

#include <stdint.h>

#include "check.h"

extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss, _estack;

int main(void);
void initialise_monitor_handles(void);

#define SYS_WRITE0 0x04
#define SYS_EXIT 0x18
#define ADP_Stopped_ApplicationExit 0x20026
#define ADP_Stopped_RunTimeErrorUnknown 0x20023

static int semihost_call(int op, const void *arg)
{
	register int r0 __asm__("r0") = op;
	register const void *r1 __asm__("r1") = arg;
	__asm__ volatile("bkpt 0xab" : "+r"(r0) : "r"(r1) : "memory");
	return r0;
}

void platform_init(void)
{
	/* Reset_Handler already set up the semihosting console. */
}

void platform_exit(int status)
{
	/* On AArch32 the SYS_EXIT argument is the reason code itself. */
	semihost_call(SYS_EXIT, (const void *)(uintptr_t)(
		status == 0 ? ADP_Stopped_ApplicationExit :
			      ADP_Stopped_RunTimeErrorUnknown));
	for (;;)
		;
}

static void put_hex(uint32_t v)
{
	char s[11] = "0x";
	for (int i = 0; i < 8; i++)
		s[2 + i] = "0123456789abcdef"[(v >> (28 - 4 * i)) & 0xf];
	s[10] = 0;
	semihost_call(SYS_WRITE0, s);
}

/* Called with the exception stack frame (r0-r3, r12, lr, pc, xpsr). */
void fault_report(uint32_t *frame)
{
	semihost_call(SYS_WRITE0, "FAIL: HardFault at pc=");
	put_hex(frame[6]);
	semihost_call(SYS_WRITE0, " lr=");
	put_hex(frame[5]);
	semihost_call(SYS_WRITE0, "\n");
	platform_exit(1);
}

__attribute__((naked)) static void Fault_Handler(void)
{
	/* Thumb-1 only, so this also assembles for ARMv6-M. */
	__asm__ volatile("mrs r0, msp\n"
			 "ldr r1, =fault_report\n"
			 "bx r1\n");
}

static void Default_Handler(void)
{
	semihost_call(SYS_WRITE0, "FAIL: unexpected exception\n");
	platform_exit(1);
}

void Reset_Handler(void)
{
	uint32_t *src, *dst;

#if defined(__ARM_FP)
	/* Grant full access to CP10/CP11 before any FP instruction runs. */
	*(volatile uint32_t *)0xE000ED88 |= 0xFu << 20;
	__asm__ volatile("dsb\n isb" ::: "memory");
#endif

	for (src = &_sidata, dst = &_sdata; dst < &_edata;)
		*dst++ = *src++;
	for (dst = &_sbss; dst < &_ebss;)
		*dst++ = 0;

	initialise_monitor_handles();
	platform_exit(main());
}

__attribute__((section(".isr_vector"), used))
static void (*const vectors[16])(void) = {
	(void (*)(void))&_estack,
	Reset_Handler,
	Default_Handler, /* NMI */
	Fault_Handler,   /* HardFault */
	Fault_Handler,   /* MemManage */
	Fault_Handler,   /* BusFault */
	Fault_Handler,   /* UsageFault */
	0, 0, 0, 0,
	Default_Handler, /* SVCall */
	Default_Handler, /* DebugMon */
	0,
	Default_Handler, /* PendSV */
	Default_Handler, /* SysTick */
};
