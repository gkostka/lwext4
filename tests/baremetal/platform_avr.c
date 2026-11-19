/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Platform hooks for AVR on simavr: stdout goes to USART0, which simavr
 * prints on its console, and the machine stops with interrupts disabled
 * and the CPU asleep, which simavr treats as the end of the simulation.
 * simavr cannot pass an exit status on, so the result is the PASS/FAIL
 * line.
 */

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <stdio.h>

#include "check.h"

static int uart_putchar(char c, FILE *stream)
{
	(void)stream;
	loop_until_bit_is_set(UCSR0A, UDRE0);
	UDR0 = c;
	return 0;
}

static FILE uart_out = FDEV_SETUP_STREAM(uart_putchar, NULL, _FDEV_SETUP_WRITE);

void platform_init(void)
{
	UCSR0B = _BV(TXEN0);
	stdout = &uart_out;
}

void platform_exit(int status)
{
	(void)status;
	cli();
	sleep_enable();
	for (;;)
		sleep_cpu();
}
