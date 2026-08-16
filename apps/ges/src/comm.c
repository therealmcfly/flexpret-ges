#include "comm.h"
#include <flexpret/uart.h>
#include <flexpret/io.h>
#include <util.h>
#include <flexpret/time.h>
#include <flexpret/csrs.h>

#define SYNC0 0xAA
#define SYNC1 0x55

int recv_from_gut()
{
	uint8_t lo = uart_receive(UART1_BASE);
	uint8_t hi = uart_receive(UART1_BASE);

	int16_t value = (int16_t)((hi << 8) | lo);
	printf("rcv: %d\n", value);

	// Convert pulse to one of 8 LED bits
	uint8_t ledmask = pulse_to_ledmask(value);

	// Display on LEDs
	set_ledmask(ledmask);
	// print the value
	// printf("Pulse value: %d\n", value);

	return (int)value;
}

void send_to_gut(PmState state)
{
	if (state == PACING)
	{
		/* END MEASUREMENT */
		int end_cycle = rdcycle();
		en = rdtime();
		int end_instret = rdinstret();
		int instructions = end_instret - start_instret;
		int cycles = end_cycle - start_cycle;
		send_et_metrics((int)et_state, en - st, cycles, instructions);
		uart_send(UART1_BASE, 1);
		return;
	}
	/* END MEASUREMENT */
	en = rdtime();
	int end_instret = rdinstret();
	int end_cycle = rdcycle();
	int instructions = end_instret - start_instret;
	int cycles = end_cycle - start_cycle;
	send_et_metrics((int)et_state, en - st, cycles, instructions);
}

void send_et_metrics(int state, int et_ns, int cycles, int instructions)
{
	// sync header
	uart_send(UART2_BASE, SYNC0);
	uart_send(UART2_BASE, SYNC1);
	// Send state as 1 byte
	uart_send(UART2_BASE, (uint8_t)(state & 0xFF));
	// Send et_ns as 4 bytes (little-endian)
	for (int i = 0; i < 4; i++)
	{
		uart_send(UART2_BASE, (uint8_t)((et_ns >> (i * 8)) & 0xFF));
	}
	// Send cycles as 4 bytes (little-endian)
	for (int i = 0; i < 4; i++)
	{
		uart_send(UART2_BASE, (uint8_t)((cycles >> (i * 8)) & 0xFF));
	}
	// Send instructions as 4 bytes (little-endian)
	for (int i = 0; i < 4; i++)
	{
		uart_send(UART2_BASE, (uint8_t)((instructions >> (i * 8)) & 0xFF));
	}
}