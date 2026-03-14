#ifndef __TCP_RECEIVER_H__
#define __TCP_RECEIVER_H__

#include "byte_stream.h"
#include "reassembler.h"
#include "tcp_messages.h"
#include "wrap32.h"

typedef struct {
    byte_stream_t stream;
    reassembler_t reassembler;
    uint32_t isn;
    int has_isn;
    unsigned capacity;
} tcp_receiver_t;

// initialize a tcp receiver with caller-provided storage
void tcp_receiver_init(tcp_receiver_t *r,
                       uint8_t *stream_storage,
                       uint8_t *pending_data,
                       uint8_t *present,
                       unsigned capacity);

// process incoming sender message
void tcp_receiver_receive(tcp_receiver_t *r, tcp_sender_msg_t msg);

// generate the receiver's current ack/window/rst state
tcp_receiver_msg_t tcp_receiver_send(const tcp_receiver_t *r);

// get the underlying reassembler
reassembler_t *tcp_receiver_reassembler(tcp_receiver_t *r);

// get the underlying byte stream
byte_stream_t *tcp_receiver_stream(tcp_receiver_t *r);

#endif
