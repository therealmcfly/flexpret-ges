#include "util.h"

#include <flexpret/io.h>

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
