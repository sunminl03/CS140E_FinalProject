#include "tcp_sender.h"

static void tcp_sender_start_timer(tcp_sender_t *s) {
    s->timer_running = 1;
}

static void tcp_sender_stop_timer(tcp_sender_t *s) {
    s->timer_running = 0;
}

static void tcp_sender_reset_time(tcp_sender_t *s) {
    s->timer_elapsed_ms = 0;
}

static void tcp_sender_reset_on_ack(tcp_sender_t *s) {
    s->current_rto_ms = s->initial_rto_ms;
    s->consecutive_retx = 0;
    tcp_sender_reset_time(s);
}

static void tcp_sender_run_exp_backoff(tcp_sender_t *s) {
    s->consecutive_retx += 1;
    s->current_rto_ms *= 2;
}

static void tcp_sender_enqueue(tcp_sender_t *s, const tcp_sender_msg_t *msg, uint64_t abs_seqno) {
    assert(s->outstanding_count < TCP_SENDER_MAX_OUTSTANDING);

    tcp_outstanding_t *slot = &s->outstanding[s->outstanding_count++];
    slot->msg = *msg;
    slot->abs_seqno = abs_seqno;
    // copy payload into sender's buffer so retransmissions stay valid
    for (unsigned i = 0; i < msg->payload_len; i++)
        slot->payload_buf[i] = msg->payload[i];
    slot->msg.payload = msg->payload_len ? slot->payload_buf : 0;
}

void tcp_sender_init(tcp_sender_t *s, uint8_t *input_storage, unsigned capacity,
                     uint32_t isn, uint64_t initial_rto_ms) {
    assert(s);

    byte_stream_init(&s->input, input_storage, capacity);
    s->isn = isn;
    s->next_abs_seqno = 0;
    s->win_size = 1;
    s->initial_rto_ms = initial_rto_ms;
    s->current_rto_ms = initial_rto_ms;
    s->timer_elapsed_ms = 0;
    s->consecutive_retx = 0;
    s->timer_running = 0;
    s->fin_sent = 0;
    s->outstanding_count = 0;
}

tcp_sender_msg_t tcp_sender_make_empty_message(const tcp_sender_t *s) {
    assert(s);

    tcp_sender_msg_t msg = {
        .seqno = wrap32(s->next_abs_seqno, s->isn),
        .payload = 0,
        .payload_len = 0,
        .syn = 0,
        .fin = 0,
        .rst = byte_stream_has_error(&s->input),
    };

    return msg;
}

void tcp_sender_receive(tcp_sender_t *s, tcp_receiver_msg_t msg) {
    assert(s);

    s->win_size = msg.window_size;
    if (msg.rst || !msg.has_ackno) {
        if (msg.rst)
            byte_stream_set_error(&s->input);
        return;
    }

    uint64_t abs_ack = unwrap32(msg.ackno, s->isn, s->next_abs_seqno);
    if (abs_ack > s->next_abs_seqno)
        return;

    int new_data_acked = 0;
    // retire every fully acked segment from the front of the outstanding queue
    while (s->outstanding_count > 0) {
        tcp_outstanding_t *front = &s->outstanding[0];
        // segment whose full sequence space is now acked
        if (front->abs_seqno + tcp_sender_msg_seq_len(&front->msg) > abs_ack)
            break;

        // shift remaining segments forward by one slot
        for (unsigned i = 1; i < s->outstanding_count; i++)
            s->outstanding[i - 1] = s->outstanding[i];
        s->outstanding_count--;
        new_data_acked = 1;
    }

    if (new_data_acked) {
        tcp_sender_reset_on_ack(s);
        if (s->outstanding_count == 0)
            tcp_sender_stop_timer(s);
    }
}

void tcp_sender_push(tcp_sender_t *s, tcp_sender_tx_fn tx, void *ctx) {
    assert(s);
    assert(tx);

    uint16_t window_size = s->win_size > 0 ? s->win_size : 1;
    if (tcp_sender_sequence_numbers_in_flight(s) >= window_size)
        return;

    
    // keep filling window until run out of room or data
    uint16_t window_avail = window_size - tcp_sender_sequence_numbers_in_flight(s);
    while (window_avail > 0 && s->outstanding_count < TCP_SENDER_MAX_OUTSTANDING) {
        tcp_sender_msg_t msg = tcp_sender_make_empty_message(s);
        msg.syn = (s->next_abs_seqno == 0);

        // syn consumes one sequence number before any payload bytes
        unsigned budget = window_avail;
        if (msg.syn)
            budget--;
        if (budget > TCP_SENDER_MAX_PAYLOAD)
            budget = TCP_SENDER_MAX_PAYLOAD;

        unsigned peek_len = 0;
        const uint8_t *peek = byte_stream_peek(&s->input, &peek_len);
        if (budget > peek_len)
            budget = peek_len;

        msg.payload = peek;
        msg.payload_len = budget;
        if (budget > 0)
            byte_stream_pop(&s->input, budget);

        unsigned used = msg.syn + msg.payload_len;
        // fin also consumes sequence space, so only attach if it fits
        msg.fin = !s->fin_sent && (window_avail > used) && byte_stream_is_finished(&s->input);
        msg.rst = byte_stream_has_error(&s->input);

        if (tcp_sender_msg_seq_len(&msg) == 0)
            return;

        uint64_t abs_seqno = s->next_abs_seqno;
        if (msg.fin)
            s->fin_sent = 1;
        s->next_abs_seqno += tcp_sender_msg_seq_len(&msg);
        window_avail -= tcp_sender_msg_seq_len(&msg);

        tcp_sender_enqueue(s, &msg, abs_seqno);
        tx(ctx, &s->outstanding[s->outstanding_count - 1].msg);
        // once anything is outstanding, timer must be running
        tcp_sender_start_timer(s);
    }
}

void tcp_sender_tick(tcp_sender_t *s, uint64_t ms_since_last_tick,
                     tcp_sender_tx_fn tx, void *ctx) {
    assert(s);
    assert(tx);

    if (!s->timer_running)
        return;

    s->timer_elapsed_ms += ms_since_last_tick;
    if (s->timer_elapsed_ms < s->current_rto_ms)
        return;

    // retransmit only the oldest outstanding segment on timeout
    if (s->outstanding_count > 0)
        tx(ctx, &s->outstanding[0].msg);

    if (s->win_size > 0)
        tcp_sender_run_exp_backoff(s);
    tcp_sender_reset_time(s);
}

uint64_t tcp_sender_sequence_numbers_in_flight(const tcp_sender_t *s) {
    assert(s);

    uint64_t total = 0;
    for (unsigned i = 0; i < s->outstanding_count; i++)
        total += tcp_sender_msg_seq_len(&s->outstanding[i].msg);
    return total;
}

uint64_t tcp_sender_consecutive_retransmissions(const tcp_sender_t *s) {
    assert(s);
    return s->consecutive_retx;
}

byte_stream_t *tcp_sender_stream(tcp_sender_t *s) {
    assert(s);
    return &s->input;
}
