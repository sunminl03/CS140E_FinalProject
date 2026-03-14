#include "rpi.h"
#include "tcp_sender.h"

typedef struct {
    tcp_sender_msg_t msgs[16];
    uint8_t payloads[16][TCP_SENDER_MAX_PAYLOAD];
    unsigned count;
} tx_capture_t;

static void capture_tx(void *ctx, const tcp_sender_msg_t *msg) {
    tx_capture_t *cap = ctx;
    assert(cap->count < 16);

    tcp_sender_msg_t *dst = &cap->msgs[cap->count];
    *dst = *msg;
    for (unsigned i = 0; i < msg->payload_len; i++)
        cap->payloads[cap->count][i] = msg->payload[i];
    dst->payload = msg->payload_len ? cap->payloads[cap->count] : 0;
    cap->count++;
}

/*
expected output:
    TCP SENDER TEST START
    empty seqno: 1000
    tx count after syn: 1
    syn flag: 1
    seq in flight after syn: 0x1
    seq in flight after syn ack: 0x0
    tx count after data: 2
    data seqno: 1001
    data payload len: 3
    tx count before retx: 2
    tx count after retx: 3
    consecutive retx: 0x1
    seq in flight after data ack: 0x0
    tx count after fin: 4
    fin flag: 1
    seq in flight after fin ack: 0x0
    TCP SENDER TEST DONE
*/

void notmain(void) {
    printk("TCP SENDER TEST START\n");

    uint8_t input_storage[16];
    tcp_sender_t s;
    tx_capture_t cap = { .count = 0 };

    tcp_sender_init(&s, input_storage, sizeof input_storage, 1000, 50);

    tcp_sender_msg_t empty = tcp_sender_make_empty_message(&s);
    printk("empty seqno: %u\n", empty.seqno);
    assert(empty.seqno == 1000);

    tcp_sender_push(&s, capture_tx, &cap);
    printk("tx count after syn: %d\n", cap.count);
    printk("syn flag: %d\n", cap.msgs[0].syn);
    printk("seq in flight after syn: %llx\n", tcp_sender_sequence_numbers_in_flight(&s));
    assert(cap.count == 1);
    assert(cap.msgs[0].syn == 1);
    assert(tcp_sender_sequence_numbers_in_flight(&s) == 1);

    tcp_receiver_msg_t rxmsg = {
        .has_ackno = 1,
        .ackno = 1001,
        .window_size = 5,
        .rst = 0,
    };
    tcp_sender_receive(&s, rxmsg);
    printk("seq in flight after syn ack: %llx\n", tcp_sender_sequence_numbers_in_flight(&s));
    assert(tcp_sender_sequence_numbers_in_flight(&s) == 0);

    byte_stream_push(tcp_sender_stream(&s), (const uint8_t *)"abc", 3);
    tcp_sender_push(&s, capture_tx, &cap);
    printk("tx count after data: %d\n", cap.count);
    printk("data seqno: %u\n", cap.msgs[1].seqno);
    printk("data payload len: %d\n", cap.msgs[1].payload_len);
    assert(cap.count == 2);
    assert(cap.msgs[1].seqno == 1001);
    assert(cap.msgs[1].payload_len == 3);

    tcp_sender_tick(&s, 49, capture_tx, &cap);
    printk("tx count before retx: %d\n", cap.count);
    assert(cap.count == 2);

    tcp_sender_tick(&s, 1, capture_tx, &cap);
    printk("tx count after retx: %d\n", cap.count);
    printk("consecutive retx: %llx\n", tcp_sender_consecutive_retransmissions(&s));
    assert(cap.count == 3);
    assert(tcp_sender_consecutive_retransmissions(&s) == 1);

    rxmsg.ackno = 1004;
    tcp_sender_receive(&s, rxmsg);
    printk("seq in flight after data ack: %llx\n", tcp_sender_sequence_numbers_in_flight(&s));
    assert(tcp_sender_sequence_numbers_in_flight(&s) == 0);

    byte_stream_close(tcp_sender_stream(&s));
    tcp_sender_push(&s, capture_tx, &cap);
    printk("tx count after fin: %d\n", cap.count);
    printk("fin flag: %d\n", cap.msgs[3].fin);
    assert(cap.count == 4);
    assert(cap.msgs[3].fin == 1);

    rxmsg.ackno = 1005;
    tcp_sender_receive(&s, rxmsg);
    printk("seq in flight after fin ack: %llx\n", tcp_sender_sequence_numbers_in_flight(&s));
    assert(tcp_sender_sequence_numbers_in_flight(&s) == 0);

    printk("TCP SENDER TEST DONE\n");
    clean_reboot();
}
