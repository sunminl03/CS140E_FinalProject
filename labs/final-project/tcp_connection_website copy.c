#include "tcp_connection.h"
#include "ip.h"
#include "net_util.h"

static tcp_connection_t g_tcp_listener;
static int g_tcp_listener_init = 0;
static tcp_output_fn_t g_tcp_output_fn = ip_send; // global tcp output function
static int g_tcp_led_init = 0;
static int g_tcp_led_on = 0;

// take sender output and package it into a real outgoing TCP segment
static void tcp_connection_queue_sender_msg(tcp_connection_t *c, const tcp_sender_msg_t *msg) {
    assert(c);
    assert(msg);
    assert(c->pending_count < TCP_CONNECTION_MAX_PENDING);

    tcp_connection_segment_t *seg = &c->pending[c->pending_count++];
    tcp_receiver_msg_t rxmsg = tcp_receiver_send(&c->rx);

    seg->hdr.src_port = c->local_port;
    seg->hdr.dst_port = c->remote_port;
    seg->hdr.seqno = msg->seqno;
    seg->hdr.ackno = rxmsg.has_ackno ? rxmsg.ackno : 0;
    seg->hdr.data_offset = TCP_HDR_LEN / 4;
    seg->hdr.flags = (rxmsg.has_ackno ? TCP_ACK : 0)
        | (msg->syn ? TCP_SYN : 0)
        | (msg->fin ? TCP_FIN : 0)
        | (msg->rst ? TCP_RST : 0);
    seg->hdr.window = rxmsg.window_size;
    seg->hdr.checksum = 0;
    seg->hdr.urgent_ptr = 0;
    seg->hdr.options = 0;
    seg->hdr.options_len = 0;
    seg->hdr.payload_len = msg->payload_len;

    if (msg->payload_len > 0) {
        for (unsigned i = 0; i < msg->payload_len; i++)
            seg->payload_buf[i] = msg->payload[i];
        seg->hdr.payload = seg->payload_buf;
    } else {
        seg->hdr.payload = 0;
    }
}

static void tcp_connection_tx_callback(void *ctx, const tcp_sender_msg_t *msg) {
    tcp_connection_queue_sender_msg(ctx, msg);
}

static void tcp_connection_reset_streams(tcp_connection_t *c) {
    assert(c);

    tcp_receiver_init(&c->rx,
                      c->rx_stream_storage,
                      c->rx_pending_data,
                      c->rx_present,
                      c->rx_capacity);
    tcp_sender_init(&c->tx,
                    c->tx_input_storage,
                    sizeof c->tx_input_storage,
                    c->isn_local,
                    TCP_CONNECTION_DEFAULT_RTO_MS);
    c->pending_count = 0;
    c->http_response_sent = 0;
}

static void tcp_connection_return_to_listen(tcp_connection_t *c) {
    assert(c);

    c->state = TCP_LISTEN;
    c->remote_ip = 0;
    c->remote_port = 0;
    tcp_connection_reset_streams(c);
}

static int tcp_connection_tuple_matches(const tcp_connection_t *c,
                                        uint32_t src_ip, uint32_t dst_ip,
                                        const tcp_hdr_t *h) {
    return c->remote_ip == src_ip
        && c->local_ip == dst_ip
        && c->remote_port == h->src_port
        && c->local_port == h->dst_port;
}

static int tcp_connection_segment_needs_ack(const tcp_hdr_t *h) {
    return h->payload_len > 0 || (h->flags & (TCP_SYN | TCP_FIN));
}

static void tcp_connection_try_queue_ack(tcp_connection_t *c, const tcp_hdr_t *h,
                                         unsigned pending_before_push) {
    assert(c);
    assert(h);

    if (c->pending_count != pending_before_push)
        return;

    if (!tcp_connection_segment_needs_ack(h))
        return;

    tcp_receiver_msg_t rxmsg = tcp_receiver_send(&c->rx);
    if (!rxmsg.has_ackno)
        return;

    tcp_sender_msg_t ack = tcp_sender_make_empty_message(&c->tx);
    tcp_connection_queue_sender_msg(c, &ack);
}

// update the connection state based on incoming acks
static void tcp_connection_update_state_after_ack(tcp_connection_t *c, const tcp_hdr_t *h) {
    assert(c);
    assert(h);

    if (!(h->flags & TCP_ACK))
        return;

    // SYN-ACK got acknowledged: handshake complete
    if (c->state == TCP_SYN_RCVD && tcp_sender_sequence_numbers_in_flight(&c->tx) == 0) {
        c->state = TCP_ESTABLISHED;
        return;
    }

    // we sent response + FIN and peer acknowledged it: go back to listen
    if (c->state == TCP_ESTABLISHED &&
        c->http_response_sent &&
        c->tx.fin_sent &&
        tcp_sender_sequence_numbers_in_flight(&c->tx) == 0) {
        tcp_connection_return_to_listen(c);
        return;
    }

    // peer initiated close and has now acked our FIN
    if (c->state == TCP_LAST_ACK &&
        c->tx.fin_sent &&
        tcp_sender_sequence_numbers_in_flight(&c->tx) == 0) {
        tcp_connection_return_to_listen(c);
    }
}

// write a decimal length into a buffer
static unsigned tcp_connection_append_len(char *dst, unsigned n) {
    char tmp[16];
    unsigned digits = 0;

    do {
        tmp[digits++] = '0' + (n % 10);
        n /= 10;
    } while (n > 0);

    for (unsigned i = 0; i < digits; i++)
        dst[i] = tmp[digits - 1 - i];
    return digits;
}

// return index just after CRLFCRLF if a full HTTP request is buffered, or -1
static int tcp_connection_http_request_end(const uint8_t *buf, unsigned len) {
    if (!buf)
        return -1;

    for (unsigned i = 0; i + 3 < len; i++) {
        if (buf[i] == '\r' && buf[i + 1] == '\n'
            && buf[i + 2] == '\r' && buf[i + 3] == '\n')
            return (int)(i + 4);
    }
    return -1;
}

// parse "GET /path ..." from the first request line
static int tcp_connection_parse_http_get_path(const tcp_connection_t *c,
                                              char *path_out, unsigned path_out_len) {
    unsigned len = 0;
    const uint8_t *buf = byte_stream_peek(&c->rx.stream, &len);
    unsigned p = 0;

    if (!path_out || path_out_len < 2)
        return 0;
    if (!buf || len < 6)
        return 0;
    if (tcp_connection_http_request_end(buf, len) < 0)
        return 0;

    if (buf[0] != 'G' || buf[1] != 'E' || buf[2] != 'T' || buf[3] != ' ')
        return 0;
    if (buf[4] != '/')
        return 0;

    for (unsigned i = 4; i < len; i++) {
        if (buf[i] == ' ' || buf[i] == '\r' || buf[i] == '\n')
            break;
        if (p + 1 >= path_out_len)
            return 0;
        path_out[p++] = (char)buf[i];
    }

    if (p == 0)
        return 0;

    path_out[p] = '\0';
    return 1;
}

static void tcp_connection_toggle_led(void) {
    if (!g_tcp_led_init) {
        gpio_set_output(TCP_CONNECTION_LED_PIN);
        gpio_write(TCP_CONNECTION_LED_PIN, 0);
        g_tcp_led_init = 1;
        g_tcp_led_on = 0;
    }

    g_tcp_led_on = !g_tcp_led_on;
    gpio_write(TCP_CONNECTION_LED_PIN, g_tcp_led_on);
}

// queue the http response once the request is complete
static void tcp_connection_try_send_http_response(tcp_connection_t *c) {
    static const char k_index_html[] =
        "<!doctype html>\n"
        "<html><body>\n"
        "<h1>Pi LED Control</h1>\n"
        // "<form action=\"/toggle\" method=\"get\">\n"
        // "<button type=\"submit\">Toggle LED</button>\n"
        // "</form>\n"
        "<a href=\"/toggle\">Toggle LED</a>\n"
        "</body></html>\n";

    static const char k_toggle_ok[] =
        "<!doctype html>\n"
        "<html><body>\n"
        "<p>LED toggled.</p>\n"
        "<a href=\"/\">Back</a>\n"
        "</body></html>\n";

    static const char k_not_found[] =
        "<!doctype html>\n"
        "<html><body>\n"
        "<p>not found</p>\n"
        "<a href=\"/\">Back</a>\n"
        "</body></html>\n";

    char path[32];

    assert(c);

    if (c->http_response_sent)
        return;
    if (c->state != TCP_ESTABLISHED)
        return;
    if (!tcp_connection_parse_http_get_path(c, path, sizeof path))
        return;

    c->http_response_sent = 1;

    if (strcmp(path, "/toggle") == 0) {
        tcp_connection_toggle_led();
        assert(tcp_connection_send_http_response(c, k_toggle_ok) == TCP_OK);
    } else if (strcmp(path, "/") == 0) {
        assert(tcp_connection_send_http_response(c, k_index_html) == TCP_OK);
    } else {
        assert(tcp_connection_send_http_response(c, k_not_found) == TCP_OK);
    }

    // match the HTTP "Connection: close" behavior
    byte_stream_close(tcp_sender_stream(&c->tx));
}

void tcp_connection_init(tcp_connection_t *c, unsigned rx_capacity) {
    assert(c);
    assert(rx_capacity <= TCP_CONNECTION_MAX_RX_CAPACITY);

    c->state = TCP_LISTEN;
    c->local_ip = 0;
    c->remote_ip = 0;
    c->local_port = TCP_CONNECTION_DEFAULT_PORT;
    c->remote_port = 0;
    c->isn_local = TCP_CONNECTION_DEFAULT_ISN;
    c->rx_capacity = rx_capacity;

    tcp_connection_reset_streams(c);
}

int tcp_connection_handle_segment(tcp_connection_t *c,
                                  uint32_t src_ip, uint32_t dst_ip,
                                  const tcp_hdr_t *h) {
    assert(c);
    assert(h);

    if (c->state == TCP_LISTEN) {
        if (!(h->flags & TCP_SYN) || (h->flags & TCP_ACK))
            return TCP_OK;

        c->remote_ip = src_ip;
        c->local_ip = dst_ip;
        c->remote_port = h->src_port;
        c->local_port = h->dst_port;
        tcp_connection_reset_streams(c);
        c->state = TCP_SYN_RCVD;
    }  else if (!tcp_connection_tuple_matches(c, src_ip, dst_ip, h)) {
        if (h->flags & TCP_SYN) {
            // stale old connection: accept a fresh new incoming connection
            tcp_connection_return_to_listen(c);
            c->remote_ip = src_ip;
            c->local_ip = dst_ip;
            c->remote_port = h->src_port;
            c->local_port = h->dst_port;
            tcp_connection_reset_streams(c);
            c->state = TCP_SYN_RCVD;
        } else {
            return TCP_OK;
        }
    }

    tcp_receiver_msg_t peer = {
        .has_ackno = (h->flags & TCP_ACK) != 0,
        .ackno = h->ackno,
        .window_size = h->window,
        .rst = (h->flags & TCP_RST) != 0,
    };
    tcp_sender_receive(&c->tx, peer);

    tcp_sender_msg_t inbound = {
        .seqno = h->seqno,
        .payload = h->payload,
        .payload_len = h->payload_len,
        .syn = (h->flags & TCP_SYN) != 0,
        .fin = (h->flags & TCP_FIN) != 0,
        .rst = (h->flags & TCP_RST) != 0,
    };
    tcp_receiver_receive(&c->rx, inbound);

    if ((h->flags & TCP_FIN) && c->state == TCP_ESTABLISHED) {
        byte_stream_close(tcp_sender_stream(&c->tx));
        c->state = TCP_LAST_ACK;
    }

    tcp_connection_update_state_after_ack(c, h);
    if (c->state == TCP_LISTEN && c->remote_port == 0)
        return TCP_OK;

    unsigned pending_before_push = c->pending_count;
    tcp_connection_try_send_http_response(c);
    tcp_sender_push(&c->tx, tcp_connection_tx_callback, c);
    tcp_connection_try_queue_ack(c, h, pending_before_push);

    return TCP_OK;
}

int tcp_connection_send_http_response(tcp_connection_t *c, const char *body) {
    assert(c);
    assert(body);

    if (c->state != TCP_ESTABLISHED && c->state != TCP_LAST_ACK)
        return TCP_ERR;

    unsigned body_len = strlen(body);
    char header[128];
    unsigned n = 0;

    const char *prefix =
        "HTTP/1.1 200 OK\r\n"
        "Connection: close\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: ";

    for (unsigned i = 0; prefix[i]; i++)
        header[n++] = prefix[i];
    n += tcp_connection_append_len(header + n, body_len);
    header[n++] = '\r';
    header[n++] = '\n';
    header[n++] = '\r';
    header[n++] = '\n';

    if (byte_stream_push(tcp_sender_stream(&c->tx), (const uint8_t *)header, n) != n)
        return TCP_ERR;
    if (byte_stream_push(tcp_sender_stream(&c->tx), (const uint8_t *)body, body_len) != body_len)
        return TCP_ERR;

    // do NOT call tcp_sender_push here; caller does that once
    return TCP_OK;
}

void tcp_connection_tick(tcp_connection_t *c, uint64_t ms_since_last_tick) {
    assert(c);
    tcp_sender_tick(&c->tx, ms_since_last_tick, tcp_connection_tx_callback, c);
}

unsigned tcp_connection_pending_segments(const tcp_connection_t *c) {
    assert(c);
    return c->pending_count;
}

int tcp_connection_pop_segment(tcp_connection_t *c, tcp_connection_segment_t *seg) {
    assert(c);
    assert(seg);

    if (c->pending_count == 0)
        return TCP_ERR;

    *seg = c->pending[0];
    for (unsigned i = 1; i < c->pending_count; i++)
        c->pending[i - 1] = c->pending[i];
    c->pending_count--;
    return TCP_OK;
}

void tcp_set_output_fn(tcp_output_fn_t fn) {
    g_tcp_output_fn = fn ? fn : ip_send;
}

static int tcp_connection_send_pending(tcp_connection_t *c) {
    uint8_t segbuf[TCP_HDR_LEN + TCP_SENDER_MAX_PAYLOAD];
    tcp_connection_segment_t seg;

    while (tcp_connection_pending_segments(c) > 0) {
        if (tcp_connection_pop_segment(c, &seg) < 0)
            return TCP_ERR;

        seg.hdr.checksum = 0;
        int seg_len = tcp_build(&seg.hdr, seg.hdr.payload, seg.hdr.payload_len,
                                segbuf, sizeof segbuf);
        if (seg_len < 0)
            return seg_len;

        uint16_t cksum = tcp_checksum(c->local_ip, c->remote_ip, segbuf, (unsigned)seg_len);
        put_be16(&segbuf[16], cksum);

        int r = g_tcp_output_fn(c->local_ip, c->remote_ip, IP_PROTO_TCP,
                                segbuf, (unsigned)seg_len);
        if (r < 0)
            return r;
    }

    return TCP_OK;
}

int tcp_handle_packet(uint32_t src_ip, uint32_t dst_ip, const uint8_t *seg, unsigned seg_len) {
    tcp_hdr_t hdr;

    if (!seg)
        return TCP_ERR;

    if (!g_tcp_listener_init) {
        tcp_connection_init(&g_tcp_listener, TCP_CONNECTION_MAX_RX_CAPACITY);
        g_tcp_listener_init = 1;
    }

    if (tcp_checksum(src_ip, dst_ip, seg, seg_len) != 0)
        return TCP_BAD_CKSUM;

    int r = tcp_parse(seg, seg_len, &hdr);
    if (r < 0)
        return r;

    r = tcp_connection_handle_segment(&g_tcp_listener, src_ip, dst_ip, &hdr);
    if (r < 0)
        return r;

    return tcp_connection_send_pending(&g_tcp_listener);
}