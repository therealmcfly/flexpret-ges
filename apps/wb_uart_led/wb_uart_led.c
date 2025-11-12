#include <stdint.h>
#include <flexpret/io.h>
#include <flexpret/assert.h>
#include <flexpret/uart.h>

int main(void)
{
	gpo_set_ledmask(0x55);
	while (1)
	{
		uint8_t byte = uart_receive(UART0_BASE);
		gpo_set_ledmask(byte);
		uart_send(UART0_BASE, byte);
	}
}
