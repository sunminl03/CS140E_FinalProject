#ifndef __BYTE_STREAM_H__
#define __BYTE_STREAM_H__

#include "rpi.h"

typedef struct {
    uint8_t *buf;
    unsigned capacity;
    unsigned buffered;
    uint64_t bytes_pushed;
    uint64_t bytes_popped;
    int closed;
    int error;
} byte_stream_t;

// init a byte stream with caller-provided storage
void byte_stream_init(byte_stream_t *s, uint8_t *buf, unsigned capacity);

// push as many bytes as fit and return the count stored
unsigned byte_stream_push(byte_stream_t *s, const uint8_t *data, unsigned len);

// returns pointer to the beginning of the buffer and optionally its length
const uint8_t *byte_stream_peek(const byte_stream_t *s, unsigned *len);

// remove up to len bytes from the front of the stream
void byte_stream_pop(byte_stream_t *s, unsigned len);

// mark the stream as closed for further writes
void byte_stream_close(byte_stream_t *s);

// mark the stream as having an error
void byte_stream_set_error(byte_stream_t *s);

// return whether the stream has been closed
int byte_stream_is_closed(const byte_stream_t *s);

// return whether the stream is closed and empty
int byte_stream_is_finished(const byte_stream_t *s);

// return whether the stream has seen an error
int byte_stream_has_error(const byte_stream_t *s);

// return how many bytes are currently buffered
unsigned byte_stream_bytes_buffered(const byte_stream_t *s);

// return how much capacity remains for new writes
unsigned byte_stream_available_capacity(const byte_stream_t *s);

#endif
