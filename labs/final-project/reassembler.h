#ifndef __REASSEMBLER_H__
#define __REASSEMBLER_H__

#include "byte_stream.h"

typedef struct {
    byte_stream_t *output;
    uint8_t *pending_data;
    uint8_t *present;
    unsigned capacity;
    unsigned pending_count;
    int has_last;
    uint64_t last_index;
} reassembler_t;

// initialize a reassembler with caller-provided pending storage
void reassembler_init(reassembler_t *r, byte_stream_t *stream,
                      uint8_t *pending_data, uint8_t *present, unsigned capacity);

// insert bytes at into correct index and flush any contiguous prefix
void reassembler_insert(reassembler_t *r, uint64_t first_index,
                        const uint8_t *data, unsigned len, int is_last_substring);

// return how many bytes are currently stored in the reassembler itself
unsigned reassembler_count_bytes_pending(const reassembler_t *r);

#endif
