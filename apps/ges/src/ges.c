#include <stdint.h>
#include <flexpret/io.h>

#include "global.h"
#include "util.h"
#include "comm.h"
#include "ring_buffer.h"

#include <flexpret/time.h>
#include <flexpret/csrs.h>
#include <inttypes.h>

// Global Variables
RingBuffer sig_rb;
int sig_buff[SIG_BUFF_SIZE];
int snapshot_buff[SIG_BUFF_SIZE];
PmState state = LEARNING; // Initial state is Learning
int sig_idx = 1;
int time_counter = 0;
int lri_counter = 0;
int gri_counter = 0;
int activation_flag = 0;
int lowest_slope_sum = 0;
int lowest_slope_count = 0;
int detection_threshold = 0;

// // ETA measurement variables
// int st = 0;
// int en = 0;
// PmState et_state;
int start_instret = 0;

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
	// printf("%s\n", PmStateNames[*state]);
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

int get_lowest_slope()
{
	// slope is (y2 - y1) / (x2 - x1) = (current sample - prev sample) / g_samp_interval_ms. But since we only need to compare slopes, we can ignore the division by (x2 - x1) which is constant.
	int lowest_slope = (snapshot_buff[1] - snapshot_buff[0]);
	for (int i = 2; i < SIG_BUFF_SIZE; i++)
	{
		int slope = (snapshot_buff[i] - snapshot_buff[i - 1]);
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
	int init_value = recv_from_gut();
	printf("Initial value received: %d. GES Start!\n", init_value);

	// Set initial conditions ..
	rb_init(&sig_rb, sig_buff, 50); // Initialize ring buffer

	while (sig_rb.is_full == 0) // initial fill of ring buffer
	{
		int new_sample = (int)recv_from_gut();
		rb_push_sample(&sig_rb, new_sample);
		sig_idx++;
		time_counter += SAMPLING_INTERVAL_MS;
	}

	state = LEARNING;

	while (1)
	{

		// 1. Sense : Acquire EGM signal from GI Model
		int new_sample = (int)recv_from_gut();

		rb_push_sample(&sig_rb, new_sample);
		sig_idx++;

		// 2. Process : Activation detection and pacing decision

		// 2.1 : Process Signal
		// 2.1.1 : Take snapshot of ring buffer
		if (!rb_snapshot(&sig_rb, snapshot_buff, BUFFER_OFFSET))
		{
			printf("\nError taking snapshot of ring buffer.\n");
			return 1; // Return error
		}

		start_instret = rdinstret();
		st = rdtime();
		et_state = state;
		/* start MEASUREMENT */

		// 2.1.2 : Get lowest slope from snapshot
		int lowest_slope = get_lowest_slope();

		// 2.2 : State Machine for Pacing Decision
		switch (state)
		{

		case LEARNING:
			if (time_counter < LEARN_TIME_MS)
			{
				// Self Looping in LEARNING state

				// accumulate lowest slope values
				lowest_slope_sum += (int)lowest_slope;
				lowest_slope_count++;
			}
			else
			{
				// Transition 1 : LEARNING -> DETECTING

				// calculate and set detection threshold
				// detection_threshold = (int16_t)(lowest_slope_sum / lowest_slope_count) * 4.5; // %%%%%% why 4.5?
				// detection_threshold = (int16_t)(lowest_slope_sum / lowest_slope_count);
				detection_threshold = lowest_slope_sum >> 13; // divide by 8192

				// reset LRI counter and set state to DETECTING
				lri_counter = 0;
				state = DETECTING;
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
					lri_counter = 0;
					gri_counter = 0;
					state = IGNORING;
				}
			}
			else
			{
				// Transition 3 : DETECTING -> PACING
				state = PACING;
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
				state = DETECTING;
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
				lri_counter = 0;
				gri_counter = 0;
				state = IGNORING;
			}
			break;

		default:
			// printf("Error:Invalid state %d in switch statement encountered in main loop\n", state);
			return 1;
			break;
		}

		// 2.3 : Update counters
		time_counter += SAMPLING_INTERVAL_MS;
		lri_counter += SAMPLING_INTERVAL_MS;
		gri_counter += SAMPLING_INTERVAL_MS;
		// 3. Actuate : Send stimulation signal based on pacing decision

		// /* END MEASUREMENT */
		// en = rdtime();
		// uint32_t end_instret = rdinstret();
		// uint32_t instructions = end_instret - start_instret;
		// send_et_metrics((int)et_state, en - st, instructions);

		send_to_gut(PACING);

		// int8_t value = !gpi_read_2();

		// if (state == PACING || value == 1)
		// {
		// 	send_to_gut(1);
		// 	// printf("Pacing signal sent to GUT.\n");
		// }

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
