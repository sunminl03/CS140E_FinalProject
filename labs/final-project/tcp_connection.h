#ifndef __TCP_CONNECTION_H__
#define __TCP_CONNECTION_H__

#include "tcp.h"
#include "tcp_receiver.h"
#include "tcp_sender.h"

#define TCP_CONNECTION_DEFAULT_PORT 80
#define TCP_CONNECTION_DEFAULT_ISN 5000
#define TCP_CONNECTION_DEFAULT_RTO_MS 50
#define TCP_CONNECTION_MAX_RX_CAPACITY 1024
#define TCP_CONNECTION_TX_CAPACITY 1024
#define TCP_CONNECTION_MAX_PENDING 32

typedef enum {
    TCP_CLOSED = 0,
    TCP_LISTEN,
    TCP_SYN_RCVD,
    TCP_ESTABLISHED,
    TCP_LAST_ACK,
} tcp_state_t;

typedef struct {
    tcp_hdr_t hdr;
    uint8_t payload_buf[TCP_SENDER_MAX_PAYLOAD];
} tcp_connection_segment_t;

typedef int (*tcp_output_fn_t)(uint32_t src, uint32_t dst, uint8_t protocol,
                               const uint8_t *payload, unsigned payload_len);

typedef struct {
    tcp_state_t state; // current state of connection

    uint32_t local_ip; // our ip once a peer has selected this listener
    uint32_t remote_ip; // peer ip for the active connection
    uint16_t local_port; // listening/service port on the pi
    uint16_t remote_port; // peer source port for the active connection

    uint32_t isn_local; // initial seqno we use when sending syn
    unsigned rx_capacity; // receive-side byte capacity for this connection

    tcp_receiver_t rx; // receiver/reassembly side of tcp
    tcp_sender_t tx; // sender/retransmission side of tcp

    uint8_t rx_stream_storage[TCP_CONNECTION_MAX_RX_CAPACITY]; // assembled inbound bytes
    uint8_t rx_pending_data[TCP_CONNECTION_MAX_RX_CAPACITY]; // out-of-order inbound bytes
    uint8_t rx_present[TCP_CONNECTION_MAX_RX_CAPACITY]; // bitmap for pending inbound bytes
    uint8_t tx_input_storage[TCP_CONNECTION_TX_CAPACITY]; // outbound app data waiting to send

    tcp_connection_segment_t pending[TCP_CONNECTION_MAX_PENDING]; // reply segments waiting to go out
    unsigned pending_count; // number of queued reply segments
    int http_response_sent; // flag to keep the server from replying twice
} tcp_connection_t;

// initialize open connection that starts in listen
void tcp_connection_init(tcp_connection_t *c, unsigned rx_capacity);

// handle one parsed tcp segment and queue any reply segments
int tcp_connection_handle_segment(tcp_connection_t *c, uint32_t src_ip, uint32_t dst_ip, const tcp_hdr_t *h);

// queue a http response body on the sender side
int tcp_connection_send_http_response(tcp_connection_t *c, const char *body);

// advance retransmission timers and queue resend traffic if needed
void tcp_connection_tick(tcp_connection_t *c, uint64_t ms_since_last_tick);

// return number of reply segments are waiting to be sent
unsigned tcp_connection_pending_segments(const tcp_connection_t *c);

// pop the oldest queued reply segment
int tcp_connection_pop_segment(tcp_connection_t *c, tcp_connection_segment_t *seg);

// override how tcp sends completed segments for tests or alternate backends
void tcp_set_output_fn(tcp_output_fn_t fn);

// parse, handle, and send one incoming tcp segment on the shared listener
int tcp_handle_packet(uint32_t src_ip, uint32_t dst_ip, const uint8_t *seg, unsigned seg_len);

#endif
