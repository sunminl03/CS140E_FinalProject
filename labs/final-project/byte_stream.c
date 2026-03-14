#include "byte_stream.h"

void byte_stream_init(byte_stream_t *s, uint8_t *buf, unsigned capacity) {
    assert(s);
    s->buf = buf;
    s->capacity = capacity;
    s->buffered = 0;
    s->bytes_pushed = 0;
    s->bytes_popped = 0;
    s->closed = 0;
    s->error = 0;
}

unsigned byte_stream_push(byte_stream_t *s, const uint8_t *data, unsigned len) {
    assert(s);

    if (!data && len > 0)
        return 0;

    if (s->closed)
        return 0;

    unsigned n = len;
    if (n > byte_stream_available_capacity(s))
        n = byte_stream_available_capacity(s);

    for (unsigned i = 0; i < n; i++)
        s->buf[s->buffered + i] = data[i];

    s->buffered += n;
    s->bytes_pushed += n;
    return n;
}

const uint8_t *byte_stream_peek(const byte_stream_t *s, unsigned *len) {
    assert(s);

    if (len)
        *len = s->buffered;

    return s->buffered ? s->buf : 0;
}

void byte_stream_pop(byte_stream_t *s, unsigned len) {
    assert(s);

    if (len > s->buffered)
        len = s->buffered;

    for (unsigned i = len; i < s->buffered; i++)
        s->buf[i - len] = s->buf[i];

    s->buffered -= len;
    s->bytes_popped += len;
}

void byte_stream_close(byte_stream_t *s) {
    assert(s);
    s->closed = 1;
}

void byte_stream_set_error(byte_stream_t *s) {
    assert(s);
    s->error = 1;
}

int byte_stream_is_closed(const byte_stream_t *s) {
    assert(s);
    return s->closed;
}

int byte_stream_is_finished(const byte_stream_t *s) {
    assert(s);
    return s->closed && s->buffered == 0;
}

int byte_stream_has_error(const byte_stream_t *s) {
    assert(s);
    return s->error;
}

unsigned byte_stream_bytes_buffered(const byte_stream_t *s) {
    assert(s);
    return s->buffered;
}

unsigned byte_stream_available_capacity(const byte_stream_t *s) {
    assert(s);
    return s->capacity - s->buffered;
}
