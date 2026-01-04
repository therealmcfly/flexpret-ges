#include <stdint.h>
#include <flexpret/io.h>

#include "const.h"
#include "util.h"
#include "comm.h"
#include "ring_buffer.h"

#include <flexpret/time.h>
#include <inttypes.h>

// Global Variables
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

// ETA measurement variables
uint64_t st = 0;
uint64_t en = 0;

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

void set_state(PmState *state, PmState new_state)
{
	*state = new_state;
	printf("%s\n", PmStateNames[*state]);
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
	fp_print_string("--------------- GES on FlexPRET Start --------------\n");
	set_ledmask(0xFF); // Set all LEDs on
	printf("Waiting for initial value from GUT...\n");
	int16_t init_value = recv_from_gut();
	printf("Initial value received: %d. GES Start!\n", init_value);

	// Set initial conditions
	rb_init(&sig_rb, sig_buff, 50); // Initialize ring buffer
	set_state(&state, LEARNING);

	while (1)
	{

		// 1. Sense : Acquire EGM signal from GI Model
		int16_t new_sample = recv_from_gut();

		st = rdtime64();
		/* start MEASUREMENT */

		rb_push_sample(&sig_rb, new_sample);
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

		// 2.2 : State Machine for Pacing Decision
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
				// Transition 1 : LEARNING -> DETECTING

				// calculate and set detection threshold
				// detection_threshold = (int16_t)(lowest_slope_sum / lowest_slope_count) * 4.5; // %%%%%% why 4.5?
				detection_threshold = (int16_t)(lowest_slope_sum / lowest_slope_count);

				// reset LRI counter and set state to DETECTING
				reset_counter(&lri_counter);
				set_state(&state, DETECTING);
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
					set_state(&state, IGNORING);
				}
			}
			else
			{
				// Transition 3 : DETECTING -> PACING
				set_state(&state, PACING);
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
				set_state(&state, DETECTING);
			}
			break;

		case PACING:
			// detect activation
			detect_activation(lowest_slope);

			if (activation_flag == 0)
			{
				// Transition 1 : PACING -> PACING (self-loop)

				// What happens when there is no activation detected? Currently nothing, will stay in this state forever. Need to add a timeout?
			}
			else
			{
				// Transition 2 : PACING -> IGNORING
				// reset GRI and LRI and set state to IGNORING
				reset_counter(&lri_counter);
				reset_counter(&gri_counter);
				set_state(&state, IGNORING);
			}
			break;

		default:
			printf("Error:Invalid state %d in switch statement encountered in main loop\n", state);
			return 1;
			break;
		}

		// 2.3 : Update counters
		increment_counter(&time_counter);
		increment_counter(&lri_counter);
		increment_counter(&gri_counter);

		// 3. Actuate : Send stimulation signal based on pacing decision

		int8_t value = !gpi_read_2();

		if (state == PACING || value == 1)
		{
			send_to_gut(1);
			// printf("Pacing signal sent to GUT.\n");
		}

		/* END MEASUREMENT */
		en = rdtime64();
		send_et_cycle((int)state, en - st);
		// int8_t value2 = !gpi_read_3();
		// if (value2 == 1)
		// {
		// 	// If KEY1 is pressed, print

		// 	uint64_t et = en - st;

		// 	printf("ST:%u %u\n", (uint32_t)(st >> 32), (uint32_t)st);
		// 	printf("EN:%u %u\n", (uint32_t)(en >> 32), (uint32_t)en);
		// 	printf("ET:%u %u\n", (uint32_t)(et >> 32), (uint32_t)et);
		// }
	}

	return 0;
}
