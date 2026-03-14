#include "rpi.h"
#include "tcp_receiver.h"

/*
expected output:
    TCP RECEIVER TEST START
    has_ack before syn: 0
    has_ack after non-syn: 0
    ack after syn: 1001
    window after syn: 8
    ack after abc: 1004
    buffered after abc: 3
    ack after out-of-order: 1004
    pending after out-of-order: 2
    ack after gap fill: 1009
    buffered after gap fill: 8
    ack after fin: 1012
    is_closed after fin: 1
    rst after rst: 1
    TCP RECEIVER TEST DONE
*/

void notmain(void) {
    printk("TCP RECEIVER TEST START\n");

    uint8_t stream_storage[8];
    uint8_t pending_data[8];
    uint8_t present[8];
    tcp_receiver_t rx;

    tcp_receiver_init(&rx, stream_storage, pending_data, present, sizeof stream_storage);

    tcp_receiver_msg_t out = tcp_receiver_send(&rx);
    printk("has_ack before syn: %d\n", out.has_ackno);
    assert(out.has_ackno == 0);

    tcp_sender_msg_t seg = {
        .seqno = 1001,
        .payload = (const uint8_t *)"abc",
        .payload_len = 3,
        .syn = 0,
        .fin = 0,
        .rst = 0,
    };
    tcp_receiver_receive(&rx, seg);
    out = tcp_receiver_send(&rx);
    printk("has_ack after non-syn: %d\n", out.has_ackno);
    assert(out.has_ackno == 0);

    seg.seqno = 1000;
    seg.payload = 0;
    seg.payload_len = 0;
    seg.syn = 1;
    tcp_receiver_receive(&rx, seg);
    out = tcp_receiver_send(&rx);
    printk("ack after syn: %u\n", out.ackno);
    printk("window after syn: %d\n", out.window_size);
    assert(out.has_ackno == 1);
    assert(out.ackno == 1001);
    assert(out.window_size == 8);

    seg.seqno = 1001;
    seg.payload = (const uint8_t *)"abc";
    seg.payload_len = 3;
    seg.syn = 0;
    tcp_receiver_receive(&rx, seg);
    out = tcp_receiver_send(&rx);
    printk("ack after abc: %u\n", out.ackno);
    printk("buffered after abc: %d\n", byte_stream_bytes_buffered(tcp_receiver_stream(&rx)));
    assert(out.ackno == 1004);
    assert(byte_stream_bytes_buffered(tcp_receiver_stream(&rx)) == 3);

    seg.seqno = 1007;
    seg.payload = (const uint8_t *)"gh";
    seg.payload_len = 2;
    tcp_receiver_receive(&rx, seg);
    out = tcp_receiver_send(&rx);
    printk("ack after out-of-order: %u\n", out.ackno);
    printk("pending after out-of-order: %d\n",
           reassembler_count_bytes_pending(tcp_receiver_reassembler(&rx)));
    assert(out.ackno == 1004);
    assert(reassembler_count_bytes_pending(tcp_receiver_reassembler(&rx)) == 2);

    seg.seqno = 1004;
    seg.payload = (const uint8_t *)"def";
    seg.payload_len = 3;
    tcp_receiver_receive(&rx, seg);
    out = tcp_receiver_send(&rx);
    printk("ack after gap fill: %u\n", out.ackno);
    printk("buffered after gap fill: %d\n", byte_stream_bytes_buffered(tcp_receiver_stream(&rx)));
    assert(out.ackno == 1009);
    assert(byte_stream_bytes_buffered(tcp_receiver_stream(&rx)) == 8);

    byte_stream_pop(tcp_receiver_stream(&rx), 8);

    seg.seqno = 1009;
    seg.payload = (const uint8_t *)"ij";
    seg.payload_len = 2;
    seg.fin = 1;
    tcp_receiver_receive(&rx, seg);
    out = tcp_receiver_send(&rx);
    printk("ack after fin: %u\n", out.ackno);
    printk("is_closed after fin: %d\n", byte_stream_is_closed(tcp_receiver_stream(&rx)));
    assert(out.ackno == 1012);
    assert(byte_stream_is_closed(tcp_receiver_stream(&rx)));

    seg.seqno = 2000;
    seg.payload = 0;
    seg.payload_len = 0;
    seg.fin = 0;
    seg.rst = 1;
    tcp_receiver_receive(&rx, seg);
    out = tcp_receiver_send(&rx);
    printk("rst after rst: %d\n", out.rst);
    assert(out.rst == 1);

    printk("TCP RECEIVER TEST DONE\n");
    clean_reboot();
}
