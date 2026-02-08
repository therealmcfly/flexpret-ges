#include "comm.h"
#include <flexpret/uart.h>
#include <util.h>

#define SYNC0 0xAA
#define SYNC1 0x55

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

void send_et_cycle(int state, uint64_t et_cycle)
{
	// sync header
	uart_send(UART2_BASE, SYNC0);
	uart_send(UART2_BASE, SYNC1);
	// Send state as 1 byte
	uart_send(UART2_BASE, (uint8_t)(state & 0xFF));
	// Send et_cycle as 8 bytes (little-endian)
	for (int i = 0; i < 8; i++)
	{
		uart_send(UART2_BASE, (uint8_t)((et_cycle >> (i * 8)) & 0xFF));
	}
}

void send_et_metrics(int state, uint64_t et_cycle, uint32_t instructions)
{
	// sync header
	uart_send(UART2_BASE, SYNC0);
	uart_send(UART2_BASE, SYNC1);
	// Send state as 1 byte
	uart_send(UART2_BASE, (uint8_t)(state & 0xFF));
	// Send et_cycle as 8 bytes (little-endian)
	for (int i = 0; i < 8; i++)
	{
		uart_send(UART2_BASE, (uint8_t)((et_cycle >> (i * 8)) & 0xFF));
	}
	// Send instructions as 4 bytes (little-endian)
	for (int i = 0; i < 4; i++)
	{
		uart_send(UART2_BASE, (uint8_t)((instructions >> (i * 8)) & 0xFF));
	}
}