#include <stdint.h>
#include <flexpret/uart.h>

#include "global.h"

const char *PmStateNames[] = {
		"LEARNING",
		"DETECTING",
		"IGNORING",
		"PACING"};

RingBuffer sig_rb;
int16_t sig_buff[SIG_BUFF_SIZE];
int16_t snapshot_buff[SIG_BUFF_SIZE];
PmState state = LEARNING; // Initial state is Learning
int16_t sig_idx = 1;
int time_counter = 0;
int lri_counter = 0;
int gri_counter = 0;
int activation_flag = 0;
int32_t lowest_slope_sum = 0;
int16_t lowest_slope_count = 0;
int16_t detection_threshold = 0;

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

void reset_counter(int *interval_ms)
{
	// reset the given interval to 0
	*interval_ms = 0;
	// printf("Counter reset to 0\n");
}
void increment_counter(int *interval_ms)
{
	*interval_ms += SAMPLING_INTERVAL_MS;
}

void set_state(PmState new_state)
{
	state = new_state;
	printf("%s\n", PmStateNames[state]);
}

void detect_activation(int16_t lowest_slope)
{
	if (lowest_slope < detection_threshold)
	{
		activation_flag = 1;
		// printf("Activation detected! Lowest slope: %d / Threshold: %d\n", lowest_slope, detection_threshold);
	}
	else
	{
		activation_flag = 0;
	}
}

int16_t get_lowest_slope(int16_t *sig_buffer, int buffer_size)
{
	// slope is (y2 - y1) / (x2 - x1) = (current sample - prev sample) / g_samp_interval_ms. But since we only need to compare slopes, we can ignore the division by (x2 - x1) which is constant.
	int16_t lowest_slope = (sig_buffer[1] - sig_buffer[0]);
	for (int i = 2; i < buffer_size; i++)
	{
		int16_t slope = (sig_buffer[i] - sig_buffer[i - 1]);
		if (slope < lowest_slope)
		{
			lowest_slope = slope;
		}
	}
	return lowest_slope;
}

int main()
{
	rb_init(&sig_rb, sig_buff, 50); // Initialize ring buffer
	fp_print_string("--------------- GES on FlexPRET Start --------------\n");
	gpo_write_0((0xFF >> 0) & 0b11);
	gpo_write_1((0xFF >> 2) & 0b11);
	gpo_write_2((0xFF >> 4) & 0b11);
	gpo_write_3((0xFF >> 6) & 0b11);

	printf("Waiting for initial value from GUT...\n");
	int16_t init_value = recv_from_gut();
	printf("Initial value received: %d. GES Start!\n", init_value);

	set_state(LEARNING);

	while (1)
	{

		// 1. Sense : Acquire EGM signal from GI Model
		rb_push_sample(&sig_rb, recv_from_gut());
		sig_idx++;

		if (!sig_rb.is_full)
		{
			increment_counter(&time_counter);
			continue; // wait until buffer is full
		}

		// 2. Process : Activation detection and pacing decision

		// 2.1 : Process Signal
		// 2.1.1 : Take snapshot of ring buffer
		if (!rb_snapshot(&sig_rb, snapshot_buff, BUFFER_OFFSET))
		{
			printf("\nError taking snapshot of ring buffer.\n");
			return 1; // Return error
		}

		// 2.1.2 : Get lowest slope from snapshot
		int16_t lowest_slope = get_lowest_slope(snapshot_buff, SIG_BUFF_SIZE);

		switch (state)
		{

		case LEARNING:
			if (time_counter < LEARN_TIME_MS)
			{
				// Self Looping in LEARNING state

				// accumulate lowest slope values
				lowest_slope_sum += (int32_t)lowest_slope;
				lowest_slope_count++;
			}
			else
			{
				// Transition : LEARNING -> DETECTING

				// calculate and set detection threshold
				// detection_threshold = (int16_t)(lowest_slope_sum / lowest_slope_count) * 4.5; // %%%%%% why 4.5?
				detection_threshold = (int16_t)(lowest_slope_sum / lowest_slope_count);

				// reset LRI counter and set state to DETECTING
				reset_counter(&lri_counter);
				set_state(DETECTING);
			}
			break;

		case DETECTING:
			// detect activation
			detect_activation(lowest_slope);

			if (lri_counter <= LRI_THRESHOLD_MS)
			{
				if (activation_flag == 0)
				{
					// Transition 1 : DETECTING -> DETECTING (self-loop)
				}
				else
				{
					// Transition 2 : DETECTING -> IGNORING
					// reset GRI and LRI and set state to IGNORING
					reset_counter(&lri_counter);
					reset_counter(&gri_counter);
					set_state(IGNORING);
				}
			}
			else
			{
				// Transition 3 : DETECTING -> PACING
				set_state(PACING);
			}

			break;

		case IGNORING:
			if (gri_counter <= GRI_THRESHOLD_MS)
			{
				// Self Looping in IGNORING state
			}
			else
			{
				// Transition 1 : IGNORING -> DETECTING
				set_state(DETECTING);
			}
			break;

		case PACING:
			// detect activation
			detect_activation(lowest_slope);

			if (activation_flag == 0)
			{
				// Transition 1 : PACING -> PACING (self-loop)
			}
			else
			{
				// Transition 2 : PACING -> IGNORING
				// reset GRI and LRI and set state to IGNORING
				reset_counter(&lri_counter);
				reset_counter(&gri_counter);
				set_state(IGNORING);
			}
			break;

		default:
			printf("Error:Invalid state %d in switch statement encountered in main loop\n", state);
			return 1;
			break;
		}

		// increment counters
		increment_counter(&time_counter);
		increment_counter(&lri_counter);
		increment_counter(&gri_counter);

		// 3. Actuate : Send stimulation signal based on pacing decision

		int8_t value = !gpi_read_2();

		if (state == PACING || value == 1)
		{
			uart_send(UART1_BASE, 1);
			// printf("Pacing signal sent to GUT.\n");
		}
	}

	return 0;
}
