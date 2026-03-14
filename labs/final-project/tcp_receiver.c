#include "tcp_receiver.h"

void tcp_receiver_init(tcp_receiver_t *r,
                       uint8_t *stream_storage,
                       uint8_t *pending_data,
                       uint8_t *present,
                       unsigned capacity) {
    assert(r);

    byte_stream_init(&r->stream, stream_storage, capacity);
    reassembler_init(&r->reassembler, &r->stream, pending_data, present, capacity);
    r->isn = 0;
    r->has_isn = 0;
    r->capacity = capacity;
}

void tcp_receiver_receive(tcp_receiver_t *r, tcp_sender_msg_t msg) {
    assert(r);

    if (msg.rst) {
        byte_stream_set_error(&r->stream);
        return;
    }

    if (msg.syn) {
        if (!r->has_isn) {
            // first syn defines the sender's isn
            r->isn = msg.seqno;
            r->has_isn = 1;
        }
    } else if (!r->has_isn || msg.seqno == r->isn) {
        // ignore data before syn, ignore bare reuse of the isn
        return;
    }

    // convert wrapped tcp seqno into an absolute byte position
    uint64_t absolute_seqno = unwrap32(msg.seqno, r->isn, r->stream.bytes_pushed);
    uint64_t stream_index = absolute_seqno > 0 ? absolute_seqno - 1 : 0;

    reassembler_insert(&r->reassembler, stream_index, msg.payload, msg.payload_len, msg.fin);
}

tcp_receiver_msg_t tcp_receiver_send(const tcp_receiver_t *r) {
    assert(r);

    tcp_receiver_msg_t msg = {
        .has_ackno = 0,
        .ackno = 0,
        .window_size = 0,
        .rst = byte_stream_has_error(&r->stream),
    };

    if (r->has_isn) {
        // ack covers the syn byte, all assembled data bytes, and fin if assembled
        uint64_t ack_abs = 1 + r->stream.bytes_pushed + byte_stream_is_closed(&r->stream);
        msg.has_ackno = 1;
        msg.ackno = wrap32(ack_abs, r->isn);
    }

    // receive window excludes bytes already buffered or waiting in reassembly
    unsigned available = r->capacity
        - byte_stream_bytes_buffered(&r->stream)
        - reassembler_count_bytes_pending(&r->reassembler);
    if (available > 0xFFFF)
        available = 0xFFFF;
    msg.window_size = (uint16_t)available;

    return msg;
}

reassembler_t *tcp_receiver_reassembler(tcp_receiver_t *r) {
    assert(r);
    return &r->reassembler;
}

byte_stream_t *tcp_receiver_stream(tcp_receiver_t *r) {
    assert(r);
    return &r->stream;
}
