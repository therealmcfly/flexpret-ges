#include "comm.h"
#include <flexpret/uart.h>
#include <util.h>
#include <flexpret/time.h>
#include <flexpret/csrs.h>
#include "global.h"

#define SYNC0 0xAA
#define SYNC1 0x55

// ETA measurement variables
int st = 0;
int en = 0;
PmState et_state;

int recv_from_gut()
{
	uint8_t lo = uart_receive(UART1_BASE);
	uint8_t hi = uart_receive(UART1_BASE);

	int value = (int)((hi << 8) | lo);
	// printf("Received: %d\n", value);

	// Convert pulse to one of 8 LED bits
	uint8_t ledmask = pulse_to_ledmask(value);

	// Display on LEDs
	set_ledmask(ledmask);
	// print the value
	// printf("Pulse value: %d\n", value);

	return value;
}

void send_to_gut(int value)
{
	if (value == PACING)
	{
		/* END MEASUREMENT */
		en = rdtime();
		int end_instret = rdinstret();
		int instructions = end_instret - start_instret;
		send_et_metrics((int)et_state, en - st, instructions);
		uart_send(UART1_BASE, value);
		return;
	}
	/* END MEASUREMENT */
	en = rdtime();
	int end_instret = rdinstret();
	int instructions = end_instret - start_instret;
	send_et_metrics((int)et_state, en - st, instructions);
}

void send_et_metrics(int state, int et_ns, int instructions)
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
	// Send instructions as 4 bytes (little-endian)
	for (int i = 0; i < 4; i++)
	{
		uart_send(UART2_BASE, (uint8_t)((instructions >> (i * 8)) & 0xFF));
	}
}