#ifndef GLOBAL_H
#define GLOBAL_H

#include <stdint.h>

#include <ring_buffer.h>
#include <flexpret/io.h>

typedef enum
{
	LEARNING = 0,	 // Learning state
	DETECTING = 1, // Detecting state
	IGNORING = 2,	 // Ignoring state
	PACING = 3		 // Pacing state
} PmState;

#define SIG_BUFF_SIZE 50				// Size of the signal ring buffer
#define BUFFER_OFFSET 1					// Overlap count for ring buffer
#define SAMPLING_INTERVAL_MS 10 // Sampling interval in seconds
#define LEARN_TIME_MS 60000			// Learning time in milliseconds
#define GRI_THRESHOLD_MS 12000	// Default GRI value in milliseconds
#define LRI_THRESHOLD_MS 20500	// Default LRI value in milliseconds

#endif // GLOBAL_H