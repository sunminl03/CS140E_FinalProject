#ifndef __TCP_SENDER_H__
#define __TCP_SENDER_H__

#include "byte_stream.h"
#include "tcp_messages.h"
#include "wrap32.h"

#define TCP_SENDER_MAX_PAYLOAD 512
#define TCP_SENDER_MAX_OUTSTANDING 16

typedef void (*tcp_sender_tx_fn)(void *ctx, const tcp_sender_msg_t *msg);

typedef struct {
    tcp_sender_msg_t msg;
    uint64_t abs_seqno;
    uint8_t payload_buf[TCP_SENDER_MAX_PAYLOAD];
} tcp_outstanding_t;

typedef struct {
    byte_stream_t input;
    uint32_t isn;
    uint64_t next_abs_seqno;
    uint16_t win_size;
    uint64_t initial_rto_ms;
    uint64_t current_rto_ms;
    uint64_t timer_elapsed_ms;
    uint64_t consecutive_retx;
    int timer_running;
    int fin_sent;
    tcp_outstanding_t outstanding[TCP_SENDER_MAX_OUTSTANDING];
    unsigned outstanding_count;
} tcp_sender_t;

// initialize a tcp sender with caller-provided input storage
void tcp_sender_init(tcp_sender_t *s, uint8_t *input_storage, unsigned capacity, uint32_t isn, uint64_t initial_rto_ms);

// create an empty sender message at the next seqno
tcp_sender_msg_t tcp_sender_make_empty_message(const tcp_sender_t *s);

// process an ack/window update from peer receiver
void tcp_sender_receive(tcp_sender_t *s, tcp_receiver_msg_t msg);

// push bytes from the outbound stream into one or more segments
void tcp_sender_push(tcp_sender_t *s, tcp_sender_tx_fn tx, void *ctx);

// advance the retransmission timer and resend if it expires
void tcp_sender_tick(tcp_sender_t *s, uint64_t ms_since_last_tick, tcp_sender_tx_fn tx, void *ctx);

// return how many seqnos are currently outstanding
uint64_t tcp_sender_sequence_numbers_in_flight(const tcp_sender_t *s);

// return how many consecutive retransmissions have happened
uint64_t tcp_sender_consecutive_retransmissions(const tcp_sender_t *s);

// access the input byte stream for tests and later integration
byte_stream_t *tcp_sender_stream(tcp_sender_t *s);

#endif
