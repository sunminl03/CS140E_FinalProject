#include "rpi.h"
#include "reassembler.h"

/*
expected output:
    REASSEMBLER TEST START
    pending after out-of-order: 3
    buffered after out-of-order: 0
    pending after gap fill: 0
    stream after gap fill: abcdef
    pending after overlap: 3
    stream after overlap: ghijk
    pending after duplicate: 0
    stream after duplicate: ghijk
    buffered after clipping: 10
    is_closed before fin: 0
    is_closed after fin: 1
    is_finished after drain: 1
    REASSEMBLER TEST DONE
*/

static void assert_stream_eq(byte_stream_t *s, const char *exp, unsigned len) {
    unsigned n = 0;
    const uint8_t *p = byte_stream_peek(s, &n);
    assert(n == len);
    for (unsigned i = 0; i < len; i++)
        assert(p[i] == (uint8_t)exp[i]);
}

void notmain(void) {
    printk("REASSEMBLER TEST START\n");

    uint8_t stream_storage[10];
    uint8_t pending_data[10];
    uint8_t present[10];
    byte_stream_t stream;
    reassembler_t r;

    byte_stream_init(&stream, stream_storage, sizeof stream_storage);
    reassembler_init(&r, &stream, pending_data, present, sizeof pending_data);

    reassembler_insert(&r, 3, (const uint8_t *)"def", 3, 0);
    printk("pending after out-of-order: %d\n", reassembler_count_bytes_pending(&r));
    printk("buffered after out-of-order: %d\n", byte_stream_bytes_buffered(&stream));
    assert(reassembler_count_bytes_pending(&r) == 3);
    assert(byte_stream_bytes_buffered(&stream) == 0);

    reassembler_insert(&r, 0, (const uint8_t *)"abc", 3, 0);
    printk("pending after gap fill: %d\n", reassembler_count_bytes_pending(&r));
    assert(reassembler_count_bytes_pending(&r) == 0);
    assert_stream_eq(&stream, "abcdef", 6);
    printk("stream after gap fill: abcdef\n");

    byte_stream_pop(&stream, 6);

    reassembler_insert(&r, 9, (const uint8_t *)"jk", 2, 0);
    reassembler_insert(&r, 8, (const uint8_t *)"ijk", 3, 0);
    printk("pending after overlap: %d\n", reassembler_count_bytes_pending(&r));
    assert(reassembler_count_bytes_pending(&r) == 3);

    reassembler_insert(&r, 6, (const uint8_t *)"gh", 2, 0);
    assert_stream_eq(&stream, "ghijk", 5);
    printk("stream after overlap: ghijk\n");

    reassembler_insert(&r, 8, (const uint8_t *)"ij", 2, 0);
    reassembler_insert(&r, 8, (const uint8_t *)"ij", 2, 0);
    printk("pending after duplicate: %d\n", reassembler_count_bytes_pending(&r));
    assert(reassembler_count_bytes_pending(&r) == 0);
    assert_stream_eq(&stream, "ghijk", 5);
    printk("stream after duplicate: ghijk\n");

    byte_stream_pop(&stream, 5);

    reassembler_insert(&r, 10, (const uint8_t *)"klmnopqrstuv", 12, 0);
    printk("buffered after clipping: %d\n", byte_stream_bytes_buffered(&stream));
    assert(byte_stream_bytes_buffered(&stream) == 10);
    assert_stream_eq(&stream, "lmnopqrstu", 10);

    printk("is_closed before fin: %d\n", byte_stream_is_closed(&stream));
    assert(!byte_stream_is_closed(&stream));

    byte_stream_pop(&stream, 10);

    reassembler_insert(&r, 21, 0, 0, 1);
    printk("is_closed after fin: %d\n", byte_stream_is_closed(&stream));
    assert(byte_stream_is_closed(&stream));

    printk("is_finished after drain: %d\n", byte_stream_is_finished(&stream));
    assert(byte_stream_is_finished(&stream));

    printk("REASSEMBLER TEST DONE\n");
    clean_reboot();
}
