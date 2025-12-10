#include "ring_buffer.h"
#include "global.h"

// #include <stdio.h>
#include <stddef.h> // for NULL

#define WRITE_COUNT_INIT_VAL 0
#define READY_TO_READ_INIT_VAL 1
#define IS_FULL_INIT_VAL 0

// Initialize buffer
void rb_init(RingBuffer *rb, int16_t *buffer, int16_t buffer_size)
{
	if (rb == NULL)
	{
		// perror("\nError: NULL pointer passed to rb_init\n");
	}
	rb->buffer = buffer; // Assign the provided buffer to the ring buffer
	rb->size = buffer_size;
	rb->head = rb->buffer;
	rb->tail = rb->buffer;
	rb->end = rb->buffer + rb->size;
	rb->is_full = IS_FULL_INIT_VAL;
	rb->rtr_flag = READY_TO_READ_INIT_VAL;
	rb->new_signal_count = WRITE_COUNT_INIT_VAL;
	// printf("Ring buffer initialized.\n");
}

int rb_push_sample(RingBuffer *rb, int16_t new_sample)
{
	if (rb == NULL)
	{
		// perror("Error: NULL pointer passed to rb_push_sample\n");
		return 0;
	}
	// add new_sample to head
	*(rb->head) = new_sample;

	// shift head to next index
	rb->head++;
	if (rb->head == rb->end) // if head points over the last index
	{
		rb->head = rb->buffer; // set head to 0th index
		if (rb->is_full == 0)
			rb->is_full = 1;
	}

	rb->new_signal_count++;
	return 1;
}

void rb_reset(RingBuffer *rb)
{
	if (rb == NULL)
	{
		// perror("Error: NULL pointer passed to rb_reset\n");
		return;
	}
	rb->head = rb->buffer;
	rb->tail = rb->buffer;
	rb->end = rb->buffer + rb->size;
	rb->is_full = IS_FULL_INIT_VAL;
	rb->rtr_flag = READY_TO_READ_INIT_VAL;
	rb->new_signal_count = WRITE_COUNT_INIT_VAL;
}

int rb_snapshot(RingBuffer *rb, int16_t *snapshot_buffer, int16_t offset)
{
	if (rb == NULL || snapshot_buffer == NULL)
	{
		printf("Error: NULL pointer passed to rb_snapshot\n");
		return 0;
	}

	if (offset < 0 || offset > rb->size)
	{
		printf("Error: Invalid offset in rb_snapshot\n");
		return 0;
	}
	// if (rb->rtr_flag != 1)
	// {
	// 	printf("Error: Ring buffer is not ready for snapshot\n");
	// 	return 0;
	// }

	// rb->rtr_flag = 0; // reset rtr_flag flag

	// printf("Taking snapshot of ring buffer!\n");
	// rtr_flag is reset outside this function after
	int16_t *curr_ptr = rb->tail;
	for (int i = 0; i < rb->size; i++)
	{
		snapshot_buffer[i] = *curr_ptr;
		curr_ptr++;
		if (curr_ptr == rb->end)
			curr_ptr = rb->buffer;
	}

	// tail is set to the start index of the next buffer
	rb->tail = rb->tail + offset;
	if (rb->tail >= rb->end)
	{
		rb->tail = rb->buffer + ((rb->tail - rb->end) % rb->size);
	}
	return 1;
}
