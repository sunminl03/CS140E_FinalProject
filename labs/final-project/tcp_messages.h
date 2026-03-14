#ifndef __TCP_MESSAGES_H__
#define __TCP_MESSAGES_H__

#include "rpi.h"

typedef struct {
    uint32_t seqno;
    const uint8_t *payload;
    unsigned payload_len;
    int syn;
    int fin;
    int rst;
} tcp_sender_msg_t;

typedef struct {
    int has_ackno;
    uint32_t ackno;
    uint16_t window_size;
    int rst;
} tcp_receiver_msg_t;

static inline unsigned tcp_sender_msg_seq_len(const tcp_sender_msg_t *msg) {
    return msg->syn + msg->payload_len + msg->fin;
}

#endif
