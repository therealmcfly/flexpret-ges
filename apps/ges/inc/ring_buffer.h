#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>

// Circular buffer size

typedef struct
{
	// double buffer[SIGNAL_PROCESSING_BUFFER_SIZE]; // Fixed-size buffer
	int *buffer;					// Pointer to the buffer array
	int size;							// Size of the buffer array
	int *head;						// Write pointer
	int *tail;						// Read pointer
	int *end;							// Pointer to end of the buffer(next address after the last element))
	int is_full;					// Flag to indicate if the buffer is full
	int rtr_flag;					// Flag to indicate if the buffer is ready
	int new_signal_count; // count of write after last read
	int cur_time_ms;			// Current time in milliseconds (for debugging or logging purposes)
} RingBuffer;

/**
 * Initializes the ring buffer
 *
 * @param rb Pointer to the RingBuffer to initialize
 * @param buffer Pointer to the buffer array
 * @param buffer_size Size of the buffer array
 */
void rb_init(RingBuffer *rb, int *buffer, int buffer_size);

/**
 * Pushes a new sample into the ring buffer
 *
 * @param rb Pointer to the RingBuffer to push the sample into
 * @param new_sample The new sample to be added
 * @param cur_time_ms Pointer to the current time in milliseconds (to be updated)
 * @return 1 on success, 0 on failure
 */
int rb_push_sample(RingBuffer *rb, int new_sample);

/**
 * Takes a snapshot of the ring buffer
 *
 * @param rb Pointer to the RingBuffer to take a snapshot of
 * @param snapshot_buffer Pointer to the buffer where the snapshot will be stored
 * @param offset Offset in the ring buffer from which to start taking the snapshot
 * @return 1 on success, 0 on failure
 */
int rb_snapshot(RingBuffer *rb, int *snapshot_buffer, int offset);

/**
 * Resets the ring buffer
 *
 * @param rb Pointer to the RingBuffer to reset
 */
void rb_reset(RingBuffer *rb);

#endif // RING_BUFFER_H
