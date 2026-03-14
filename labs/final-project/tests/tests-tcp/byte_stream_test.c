#include "rpi.h"
#include "byte_stream.h"

/*
expected output:
    BYTE STREAM TEST START
    initial buffered: 0
    initial capacity: 8
    pushed1: 5
    after push1 buffered: 5
    after push1 capacity: 3
    peek len: 5
    peek bytes: hello
    pushed2: 3
    after push2 buffered: 8
    after push2 capacity: 0
    bytes_pushed: 8
    after pop buffered: 5
    bytes_popped: 3
    peek after pop: lowor
    is_closed: 1
    is_finished before drain: 0
    is_finished after drain: 1
    has_error: 1
    BYTE STREAM TEST DONE
*/

void notmain(void) {
    printk("BYTE STREAM TEST START\n");

    uint8_t storage[8];
    byte_stream_t s;
    byte_stream_init(&s, storage, sizeof storage);

    printk("initial buffered: %d\n", byte_stream_bytes_buffered(&s));
    printk("initial capacity: %d\n", byte_stream_available_capacity(&s));
    assert(byte_stream_bytes_buffered(&s) == 0);
    assert(byte_stream_available_capacity(&s) == 8);

    uint8_t hello[] = { 'h', 'e', 'l', 'l', 'o' };
    unsigned pushed1 = byte_stream_push(&s, hello, sizeof hello);
    printk("pushed1: %d\n", pushed1);
    printk("after push1 buffered: %d\n", byte_stream_bytes_buffered(&s));
    printk("after push1 capacity: %d\n", byte_stream_available_capacity(&s));
    assert(pushed1 == 5);

    unsigned peek_len = 0;
    const uint8_t *peek = byte_stream_peek(&s, &peek_len);
    printk("peek len: %d\n", peek_len);
    printk("peek bytes: %c%c%c%c%c\n", peek[0], peek[1], peek[2], peek[3], peek[4]);
    assert(peek_len == 5);

    uint8_t world[] = { 'w', 'o', 'r', 'l', 'd' };
    unsigned pushed2 = byte_stream_push(&s, world, sizeof world);
    printk("pushed2: %d\n", pushed2);
    printk("after push2 buffered: %d\n", byte_stream_bytes_buffered(&s));
    printk("after push2 capacity: %d\n", byte_stream_available_capacity(&s));
    printk("bytes_pushed: %llx\n", s.bytes_pushed);
    assert(pushed2 == 3);
    assert(s.bytes_pushed == 8);

    byte_stream_pop(&s, 3);
    printk("after pop buffered: %d\n", byte_stream_bytes_buffered(&s));
    printk("bytes_popped: %llx\n", s.bytes_popped);
    assert(byte_stream_bytes_buffered(&s) == 5);
    assert(s.bytes_popped == 3);

    peek = byte_stream_peek(&s, &peek_len);
    printk("peek after pop: %c%c%c%c%c\n", peek[0], peek[1], peek[2], peek[3], peek[4]);
    assert(peek_len == 5);
    assert(peek[0] == 'l');
    assert(peek[1] == 'o');
    assert(peek[2] == 'w');
    assert(peek[3] == 'o');
    assert(peek[4] == 'r');

    byte_stream_close(&s);
    printk("is_closed: %d\n", byte_stream_is_closed(&s));
    printk("is_finished before drain: %d\n", byte_stream_is_finished(&s));
    assert(byte_stream_is_closed(&s));
    assert(!byte_stream_is_finished(&s));

    byte_stream_pop(&s, 5);
    printk("is_finished after drain: %d\n", byte_stream_is_finished(&s));
    assert(byte_stream_is_finished(&s));

    byte_stream_set_error(&s);
    printk("has_error: %d\n", byte_stream_has_error(&s));
    assert(byte_stream_has_error(&s));

    printk("BYTE STREAM TEST DONE\n");
    clean_reboot();
}
