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

int main()
{
	unsigned char recv;
	uint32_t count = 0;

	printf("--------------- GES Start --------------\n");
	set_ledmask(0xFF);

	while (1)
	{
		recv = uart_receive(UART1_BASE); // i need another FTDI USB 3.3V 5.5V to TTL Serial Adapter to do this

		// print integer16 value to console
		printf("Received byte: %d\n", recv);

		// // print binary value to console
		// printf("Received byte: 0b");
		// for (int i = 7; i >= 0; i--)
		// {
		// 	printf("%d", (recv >> i) & 0x01);
		// }
		// printf("\n");
		// echo back received byte

		// if (count++ % 100000 == 0)
		// gpo_set_ledmask(count);
		// uart_send(UART0_BASE, 0b01);
	}
}
