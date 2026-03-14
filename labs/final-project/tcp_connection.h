// tcp_connection.h
typedef enum {
    TCP_CLOSED = 0,
    TCP_SYN_RCVD,
    TCP_ESTABLISHED,
    TCP_LAST_ACK,
} tcp_state_t;

typedef struct {
    tcp_state_t state;

    uint32_t local_ip, remote_ip;
    uint16_t local_port, remote_port;

    uint32_t isn_local;
    uint64_t next_abs_seq;      // next byte to send (absolute)
    uint64_t acked_abs_seq;     // highest acked absolute seq

    tcp_receiver_t rx;

    uint8_t tx_buf[1024];
    unsigned tx_len;
} tcp_connection_t;

void tcp_connection_init(tcp_connection_t *c, unsigned rx_capacity);
int tcp_connection_handle_segment(tcp_connection_t *c,
                                  uint32_t src_ip, uint32_t dst_ip,
                                  const tcp_hdr_t *h);
int tcp_connection_send_http_response(tcp_connection_t *c, const char *body);