#include <flexpret/wb.h>
#include <flexpret/uart.h>
#include <flexpret/assert.h>

// void uart_send(uint8_t data) {
void uart_send(uint32_t base, uint8_t data)
{
	// while (UART_TX_FULL(wb_read(UART_CSR)));
	// wb_write(UART_TXD, data);
	while (UART_TX_FULL(wb_read(base + UART_CSR_OFF)))
		;
	wb_write(base + UART_TXD_OFF, data);
}

// bool uart_available(void)
bool uart_available(uint32_t base)
{
	/**
	 * The UART device has a register that contains a magic number
	 * `UART_CONST_VALUE`. The sole purpose of this is to check whether
	 * we can use the UART device.
	 *
	 */
	// return wb_read(UART_CONST_ADDR) == UART_CONST_VALUE;
	return wb_read(base + UART_CONST_OFF) == UART_CONST_VALUE;
}

// uint8_t uart_receive(void)
uint8_t uart_receive(uint32_t base)
{
	// while (!UART_DATA_READY(wb_read(UART_CSR)));
	// return wb_read(UART_RXD);
	while (!UART_DATA_READY(wb_read(base + UART_CSR_OFF)))
		;
	return wb_read(base + UART_RXD_OFF);
}
