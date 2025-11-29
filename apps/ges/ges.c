#include <stdint.h>
#include <flexpret/io.h>
#include <flexpret/uart.h>

void set_ledmask(const uint8_t byte)
{
	gpo_write_0((byte >> 0) & 0b11);
	gpo_write_1((byte >> 2) & 0b11);
	gpo_write_2((byte >> 4) & 0b11);
	gpo_write_3((byte >> 6) & 0b11);
}

uint8_t pulse_to_ledmask(int16_t value)
{
	uint8_t mask = 0;

	if (value > 0 && value >= -1000)
		mask = 0b00000001; // LED0

	else if (value >= -2000)
		mask = 0b00000010; // LED1

	else if (value >= -3000)
		mask = 0b00000100; // LED2

	else if (value >= -4000)
		mask = 0b00001000; // LED3

	else if (value >= -5000)
		mask = 0b00010000; // LED4

	else if (value >= -6000)
		mask = 0b00100000; // LED5

	else if (value >= -7000)
		mask = 0b01000000; // LED6

	else if (value >= -8000)
		mask = 0b10000000; // LED7

	else
		mask = 0b00000000; // All off

	return mask;
}

void recv_from_gut()
{
	uint8_t lo = uart_receive(UART1_BASE);
	uint8_t hi = uart_receive(UART1_BASE);

	int16_t value = (int16_t)((hi << 8) | lo);
	// printf("Received: %d\n", value);

	// Convert pulse to one of 8 LED bits
	uint8_t ledmask = pulse_to_ledmask(value);

	// Display on LEDs
	set_ledmask(ledmask);
}

void send_to_gut(int8_t value)
{
	if (value)
	{
		uart_send(UART1_BASE, value);
		printf("Sent int8 value: %d\n", value);
	}
}

int main()
{
	unsigned char recv;
	uint32_t count = 0;

	printf("--------------- GES on FlexPRET Start --------------\n");
	set_ledmask(0xFF);

	while (1)
	{

		recv_from_gut();

		send_to_gut(!gpi_read_2());
	}
}
