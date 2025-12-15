#include "comm.h"
#include <flexpret/uart.h>
#include <util.h>

int16_t recv_from_gut()
{
	uint8_t lo = uart_receive(UART1_BASE);
	uint8_t hi = uart_receive(UART1_BASE);

	int16_t value = (int16_t)((hi << 8) | lo);
	// printf("Received: %d\n", value);

	// Convert pulse to one of 8 LED bits
	uint8_t ledmask = pulse_to_ledmask(value);

	// Display on LEDs
	set_ledmask(ledmask);
	// print the value
	// printf("Pulse value: %d\n", value);

	return value;
}

void send_to_gut(int16_t value)
{
	uart_send(UART1_BASE, value);
}