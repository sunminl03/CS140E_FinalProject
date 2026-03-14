#include "reassembler.h"

// drop n assembled bytes from the front of the pending window
static void reassembler_shift_left(reassembler_t *r, unsigned n) {
    if (n == 0)
        return;

    for (unsigned i = n; i < r->capacity; i++) {
        r->pending_data[i - n] = r->pending_data[i];
        r->present[i - n] = r->present[i];
    }

    for (unsigned i = r->capacity - n; i < r->capacity; i++) {
        r->pending_data[i] = 0;
        r->present[i] = 0;
    }
}

// push the contiguous pending prefix into the output stream
static void reassembler_flush(reassembler_t *r) {
    unsigned contiguous = 0;
    while (contiguous < r->capacity && r->present[contiguous])
        contiguous++;

    if (contiguous == 0)
        return;

    unsigned pushed = byte_stream_push(r->output, r->pending_data, contiguous);
    assert(pushed == contiguous);

    r->pending_count -= contiguous;
    reassembler_shift_left(r, contiguous);
}

void reassembler_init(reassembler_t *r, byte_stream_t *stream, uint8_t *pending_data, uint8_t *present, unsigned capacity) {
    assert(r);
    assert(stream);
    assert(pending_data);
    assert(present);

    r->output = stream;
    r->pending_data = pending_data;
    r->present = present;
    r->capacity = capacity;
    r->pending_count = 0;
    r->has_last = 0;
    r->last_index = 0;

    for (unsigned i = 0; i < capacity; i++) {
        r->pending_data[i] = 0;
        r->present[i] = 0;
    }
}

void reassembler_insert(reassembler_t *r, uint64_t first_index, const uint8_t *data, unsigned len, int is_last_substring) {
    assert(r);

    if (is_last_substring) {
        r->has_last = 1;
        r->last_index = first_index + len;
    }

    if (byte_stream_is_closed(r->output))
        return;

    uint64_t first_unassembled = r->output->bytes_pushed;
    unsigned window = r->capacity - byte_stream_bytes_buffered(r->output) - r->pending_count;
    uint64_t first_unacceptable = first_unassembled + window;

    if (len > 0 && data) {
        uint64_t start = first_index;
        uint64_t end = first_index + len;

        if (start < first_unassembled)
            start = first_unassembled;

        if (end > first_unacceptable)
            end = first_unacceptable;

        if (start < end) {
            for (uint64_t abs_idx = start; abs_idx < end; abs_idx++) {
                unsigned rel = (unsigned)(abs_idx - first_unassembled);
                if (!r->present[rel]) {
                    // only count a byte the first time we learn it
                    r->present[rel] = 1;
                    r->pending_data[rel] = data[abs_idx - first_index];
                    r->pending_count++;
                }
            }
        }
    }

    // flush as much as possible into the stream
    reassembler_flush(r);

    if (r->has_last && r->output->bytes_pushed == r->last_index)
        byte_stream_close(r->output);
}

unsigned reassembler_count_bytes_pending(const reassembler_t *r) {
    assert(r);
    return r->pending_count;
}
