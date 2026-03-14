#include "rpi.h"
#include "tcp_connection.h"

/*
expected output:
    TCP CONNECTION TEST START
    state after syn: 2
    pending after syn: 1
    synack flags: 18
    synack ackno: 9001
    state after ack: 3
    pending after data: 1
    ack-only flags: 16
    ack after data: 9005
    rx buffered: 4
    pending after response: 1
    response flags: 16
    response seqno: 5001
    response first bytes: HTTP
    state after fin: 4
    pending after fin: 1
    finack flags: 17
    finack ackno: 9006
    state after final ack: 1
    TCP CONNECTION TEST DONE
*/

static tcp_hdr_t make_hdr(uint16_t src_port, uint16_t dst_port,
                          uint32_t seqno, uint32_t ackno,
                          uint8_t flags, uint16_t window,
                          const uint8_t *payload, unsigned payload_len) {
    tcp_hdr_t h = {
        .src_port = src_port,
        .dst_port = dst_port,
        .seqno = seqno,
        .ackno = ackno,
        .data_offset = TCP_HDR_LEN / 4,
        .flags = flags,
        .window = window,
        .checksum = 0,
        .urgent_ptr = 0,
        .options = 0,
        .options_len = 0,
        .payload = payload,
        .payload_len = payload_len,
    };
    return h;
}

void notmain(void) {
    printk("TCP CONNECTION TEST START\n");

    tcp_connection_t c;
    tcp_connection_init(&c, 32);

    tcp_hdr_t in = make_hdr(5000, 80, 9000, 0, TCP_SYN, 64, 0, 0);
    assert(tcp_connection_handle_segment(&c, 0x0a000001, 0x0a000002, &in) == TCP_OK);
    printk("state after syn: %d\n", c.state);
    printk("pending after syn: %d\n", tcp_connection_pending_segments(&c));
    assert(c.state == TCP_SYN_RCVD);
    assert(tcp_connection_pending_segments(&c) == 1);

    tcp_connection_segment_t out;
    assert(tcp_connection_pop_segment(&c, &out) == TCP_OK);
    printk("synack flags: %d\n", out.hdr.flags);
    printk("synack ackno: %u\n", out.hdr.ackno);
    assert(out.hdr.flags == (TCP_SYN | TCP_ACK));
    assert(out.hdr.seqno == TCP_CONNECTION_DEFAULT_ISN);
    assert(out.hdr.ackno == 9001);

    in = make_hdr(5000, 80, 9001, TCP_CONNECTION_DEFAULT_ISN + 1, TCP_ACK, 64, 0, 0);
    assert(tcp_connection_handle_segment(&c, 0x0a000001, 0x0a000002, &in) == TCP_OK);
    printk("state after ack: %d\n", c.state);
    assert(c.state == TCP_ESTABLISHED);

    in = make_hdr(5000, 80, 9001, TCP_CONNECTION_DEFAULT_ISN + 1, TCP_ACK, 64,
                  (const uint8_t *)"GET ", 4);
    assert(tcp_connection_handle_segment(&c, 0x0a000001, 0x0a000002, &in) == TCP_OK);
    printk("pending after data: %d\n", tcp_connection_pending_segments(&c));
    assert(tcp_connection_pending_segments(&c) == 1);

    assert(tcp_connection_pop_segment(&c, &out) == TCP_OK);
    printk("ack-only flags: %d\n", out.hdr.flags);
    printk("ack after data: %u\n", out.hdr.ackno);
    printk("rx buffered: %d\n", byte_stream_bytes_buffered(tcp_receiver_stream(&c.rx)));
    assert(out.hdr.flags == TCP_ACK);
    assert(out.hdr.seqno == TCP_CONNECTION_DEFAULT_ISN + 1);
    assert(out.hdr.ackno == 9005);
    assert(byte_stream_bytes_buffered(tcp_receiver_stream(&c.rx)) == 4);

    assert(tcp_connection_send_http_response(&c, "ok") == TCP_OK);
    printk("pending after response: %d\n", tcp_connection_pending_segments(&c));
    assert(tcp_connection_pending_segments(&c) == 1);

    assert(tcp_connection_pop_segment(&c, &out) == TCP_OK);
    printk("response flags: %d\n", out.hdr.flags);
    printk("response seqno: %u\n", out.hdr.seqno);
    printk("response first bytes: %c%c%c%c\n",
           out.hdr.payload[0], out.hdr.payload[1], out.hdr.payload[2], out.hdr.payload[3]);
    assert(out.hdr.flags == TCP_ACK);
    assert(out.hdr.seqno == TCP_CONNECTION_DEFAULT_ISN + 1);
    assert(out.hdr.payload_len > 4);
    assert(out.hdr.payload[0] == 'H');
    assert(out.hdr.payload[1] == 'T');
    assert(out.hdr.payload[2] == 'T');
    assert(out.hdr.payload[3] == 'P');

    in = make_hdr(5000, 80, 9005,
                  TCP_CONNECTION_DEFAULT_ISN + 1 + out.hdr.payload_len,
                  TCP_ACK, 64, 0, 0);
    assert(tcp_connection_handle_segment(&c, 0x0a000001, 0x0a000002, &in) == TCP_OK);

    in = make_hdr(5000, 80, 9005,
                  TCP_CONNECTION_DEFAULT_ISN + 1 + out.hdr.payload_len,
                  TCP_ACK | TCP_FIN, 64, 0, 0);
    assert(tcp_connection_handle_segment(&c, 0x0a000001, 0x0a000002, &in) == TCP_OK);
    printk("state after fin: %d\n", c.state);
    printk("pending after fin: %d\n", tcp_connection_pending_segments(&c));
    assert(c.state == TCP_LAST_ACK);
    assert(tcp_connection_pending_segments(&c) == 1);

    assert(tcp_connection_pop_segment(&c, &out) == TCP_OK);
    printk("finack flags: %d\n", out.hdr.flags);
    printk("finack ackno: %u\n", out.hdr.ackno);
    assert(out.hdr.flags == (TCP_FIN | TCP_ACK));
    assert(out.hdr.ackno == 9006);

    in = make_hdr(5000, 80, 9006, out.hdr.seqno + 1, TCP_ACK, 64, 0, 0);
    assert(tcp_connection_handle_segment(&c, 0x0a000001, 0x0a000002, &in) == TCP_OK);
    printk("state after final ack: %d\n", c.state);
    assert(c.state == TCP_LISTEN);

    printk("TCP CONNECTION TEST DONE\n");
    clean_reboot();
}
