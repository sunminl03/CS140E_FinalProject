#include "rpi.h"
#include "tcp_receiver.h"

/*
expected output:
    TCP RECEIVER EDGE TEST START
    ack after syn: 40001
    ack after abc: 40004
    buffered after abc: 3
    ack after duplicate abc: 40004
    buffered after duplicate abc: 3
    ack after stale aa: 40004
    pending after stale aa: 0
    ack after fin out-of-order: 40004
    is_closed after fin out-of-order: 0
    pending after fin out-of-order: 2
    ack after gap fill with fin completion: 40008
    is_closed after fin completion: 1
    buffered final: 6
    TCP RECEIVER EDGE TEST DONE
*/

void notmain(void) {
    printk("TCP RECEIVER EDGE TEST START\n");

    uint8_t stream_storage[16];
    uint8_t pending_data[16];
    uint8_t present[16];
    tcp_receiver_t rx;

    tcp_receiver_init(&rx, stream_storage, pending_data, present, sizeof stream_storage);

    tcp_sender_msg_t seg = {
        .seqno = 40000,
        .payload = 0,
        .payload_len = 0,
        .syn = 1,
        .fin = 0,
        .rst = 0,
    };
    tcp_receiver_receive(&rx, seg);

    tcp_receiver_msg_t out = tcp_receiver_send(&rx);
    printk("ack after syn: %u\n", out.ackno);
    assert(out.has_ackno == 1);
    assert(out.ackno == 40001);

    // In-order payload.
    seg.seqno = 40001;
    seg.payload = (const uint8_t *)"abc";
    seg.payload_len = 3;
    seg.syn = 0;
    tcp_receiver_receive(&rx, seg);
    out = tcp_receiver_send(&rx);
    printk("ack after abc: %u\n", out.ackno);
    printk("buffered after abc: %d\n", byte_stream_bytes_buffered(tcp_receiver_stream(&rx)));
    assert(out.ackno == 40004);
    assert(byte_stream_bytes_buffered(tcp_receiver_stream(&rx)) == 3);

    // Exact duplicate should not change ack or buffered bytes.
    seg.seqno = 40001;
    seg.payload = (const uint8_t *)"abc";
    seg.payload_len = 3;
    tcp_receiver_receive(&rx, seg);
    out = tcp_receiver_send(&rx);
    printk("ack after duplicate abc: %u\n", out.ackno);
    printk("buffered after duplicate abc: %d\n", byte_stream_bytes_buffered(tcp_receiver_stream(&rx)));
    assert(out.ackno == 40004);
    assert(byte_stream_bytes_buffered(tcp_receiver_stream(&rx)) == 3);

    // Stale bytes entirely before first_unassembled should be ignored.
    seg.seqno = 40002;
    seg.payload = (const uint8_t *)"aa";
    seg.payload_len = 2;
    tcp_receiver_receive(&rx, seg);
    out = tcp_receiver_send(&rx);
    printk("ack after stale aa: %u\n", out.ackno);
    printk("pending after stale aa: %d\n",
           reassembler_count_bytes_pending(tcp_receiver_reassembler(&rx)));
    assert(out.ackno == 40004);
    assert(reassembler_count_bytes_pending(tcp_receiver_reassembler(&rx)) == 0);

    // FIN carried on out-of-order bytes should not close until the gap is filled.
    seg.seqno = 40005;
    seg.payload = (const uint8_t *)"ef";
    seg.payload_len = 2;
    seg.fin = 1;
    tcp_receiver_receive(&rx, seg);
    out = tcp_receiver_send(&rx);
    printk("ack after fin out-of-order: %u\n", out.ackno);
    printk("is_closed after fin out-of-order: %d\n",
           byte_stream_is_closed(tcp_receiver_stream(&rx)));
    printk("pending after fin out-of-order: %d\n",
           reassembler_count_bytes_pending(tcp_receiver_reassembler(&rx)));
    assert(out.ackno == 40004);
    assert(!byte_stream_is_closed(tcp_receiver_stream(&rx)));
    assert(reassembler_count_bytes_pending(tcp_receiver_reassembler(&rx)) == 2);

    // Fill the gap and confirm ack includes FIN (+1) and stream closes.
    seg.seqno = 40004;
    seg.payload = (const uint8_t *)"d";
    seg.payload_len = 1;
    seg.fin = 0;
    tcp_receiver_receive(&rx, seg);
    out = tcp_receiver_send(&rx);
    printk("ack after gap fill with fin completion: %u\n", out.ackno);
    printk("is_closed after fin completion: %d\n",
           byte_stream_is_closed(tcp_receiver_stream(&rx)));
    printk("buffered final: %d\n", byte_stream_bytes_buffered(tcp_receiver_stream(&rx)));
    assert(out.ackno == 40008);
    assert(byte_stream_is_closed(tcp_receiver_stream(&rx)));
    assert(byte_stream_bytes_buffered(tcp_receiver_stream(&rx)) == 6);

    printk("TCP RECEIVER EDGE TEST DONE\n");
    clean_reboot();
}
