#include "rpi.h"
#include "ip.h"
#include "net_util.h"
#include "tcp.h"
#include "tcp_connection.h"

static uint32_t g_tx_src;
static uint32_t g_tx_dst;
static uint8_t g_tx_proto;
static uint8_t g_tx_payload[1600];
static unsigned g_tx_payload_len;
static unsigned g_tx_count;

static int capture_tcp_output(uint32_t src, uint32_t dst, uint8_t protocol,
                              const uint8_t *payload, unsigned payload_len) {
    g_tx_src = src;
    g_tx_dst = dst;
    g_tx_proto = protocol;
    g_tx_payload_len = payload_len;
    g_tx_count++;

    for (unsigned i = 0; i < payload_len; i++)
        g_tx_payload[i] = payload[i];
    return 0;
}

// start each check with an empty tcp output capture buffer
static void clear_tx_capture(void) {
    g_tx_src = 0;
    g_tx_dst = 0;
    g_tx_proto = 0;
    g_tx_payload_len = 0;
    g_tx_count = 0;
}

static unsigned build_tcp_segment(uint32_t src_ip, uint32_t dst_ip,
                                  const tcp_hdr_t *hdr, uint8_t *out, unsigned out_size) {
    tcp_hdr_t tmp = *hdr;
    int n = tcp_build(&tmp, hdr->payload, hdr->payload_len, out, out_size);
    assert(n > 0);
    put_be16(&out[16], tcp_checksum(src_ip, dst_ip, out, (unsigned)n));
    return (unsigned)n;
}

static unsigned build_ip_packet(uint32_t src_ip, uint32_t dst_ip,
                                const uint8_t *payload, unsigned payload_len,
                                uint8_t *out, unsigned out_size) {
    ip_hdr_t hdr = {
        .version = 4,
        .ihl = 5,
        .tos = 0,
        .total_len = 0,
        .id = 1,
        .flags = 0x02,
        .frag_offset = 0,
        .ttl = 64,
        .protocol = IP_PROTO_TCP,
        .checksum = 0,
        .src = src_ip,
        .dst = dst_ip,
        .payload = 0,
        .payload_len = 0,
    };

    int n = ip_build(&hdr, payload, payload_len, out, out_size);
    assert(n > 0);
    return (unsigned)n;
}

/*
expected output:
    TCP IP INTEGRATION TEST START
    synack flags: 18
    synack ackno: 9001
    synack seqno: 5000
    tx count after pure ack: 0
    ack-only flags: 16
    ack-only ackno: 9005
    ack-only seqno: 5001
    TCP IP INTEGRATION TEST DONE
*/

void notmain(void) {
    printk("TCP IP INTEGRATION TEST START\n");

    uint32_t host_ip = 0x0A000001;
    uint32_t pi_ip = 0x0A000002;

    uint8_t tcp_seg[1600];
    uint8_t ip_pkt[1600];
    tcp_hdr_t out_tcp;

    tcp_set_output_fn(capture_tcp_output);

    tcp_hdr_t syn = {
        .src_port = 5000,
        .dst_port = 80,
        .seqno = 9000,
        .ackno = 0,
        .data_offset = TCP_HDR_LEN / 4,
        .flags = TCP_SYN,
        .window = 64,
        .checksum = 0,
        .urgent_ptr = 0,
        .options = 0,
        .options_len = 0,
        .payload = 0,
        .payload_len = 0,
    };

    unsigned syn_len = build_tcp_segment(host_ip, pi_ip, &syn, tcp_seg, sizeof tcp_seg);
    unsigned syn_pkt_len = build_ip_packet(host_ip, pi_ip, tcp_seg, syn_len, ip_pkt, sizeof ip_pkt);

    clear_tx_capture();
    assert(ip_handle_packet(ip_pkt, syn_pkt_len) == IP_OK);
    assert(g_tx_count == 1);
    assert(g_tx_proto == IP_PROTO_TCP);
    assert(g_tx_src == pi_ip);
    assert(g_tx_dst == host_ip);
    assert(tcp_parse(g_tx_payload, g_tx_payload_len, &out_tcp) == TCP_OK);
    printk("synack flags: %d\n", out_tcp.flags);
    printk("synack ackno: %u\n", out_tcp.ackno);
    printk("synack seqno: %u\n", out_tcp.seqno);
    assert(out_tcp.flags == (TCP_SYN | TCP_ACK));
    assert(out_tcp.ackno == 9001);
    assert(out_tcp.seqno == 5000);

    tcp_hdr_t ack = {
        .src_port = 5000,
        .dst_port = 80,
        .seqno = 9001,
        .ackno = 5001,
        .data_offset = TCP_HDR_LEN / 4,
        .flags = TCP_ACK,
        .window = 64,
        .checksum = 0,
        .urgent_ptr = 0,
        .options = 0,
        .options_len = 0,
        .payload = 0,
        .payload_len = 0,
    };

    unsigned ack_len = build_tcp_segment(host_ip, pi_ip, &ack, tcp_seg, sizeof tcp_seg);
    unsigned ack_pkt_len = build_ip_packet(host_ip, pi_ip, tcp_seg, ack_len, ip_pkt, sizeof ip_pkt);

    clear_tx_capture();
    assert(ip_handle_packet(ip_pkt, ack_pkt_len) == IP_OK);
    printk("tx count after pure ack: %d\n", g_tx_count);
    assert(g_tx_count == 0);

    tcp_hdr_t data = {
        .src_port = 5000,
        .dst_port = 80,
        .seqno = 9001,
        .ackno = 5001,
        .data_offset = TCP_HDR_LEN / 4,
        .flags = TCP_ACK,
        .window = 64,
        .checksum = 0,
        .urgent_ptr = 0,
        .options = 0,
        .options_len = 0,
        .payload = (const uint8_t *)"GET ",
        .payload_len = 4,
    };

    unsigned data_len = build_tcp_segment(host_ip, pi_ip, &data, tcp_seg, sizeof tcp_seg);
    unsigned data_pkt_len = build_ip_packet(host_ip, pi_ip, tcp_seg, data_len, ip_pkt, sizeof ip_pkt);

    clear_tx_capture();
    assert(ip_handle_packet(ip_pkt, data_pkt_len) == IP_OK);
    assert(g_tx_count == 1);
    assert(tcp_parse(g_tx_payload, g_tx_payload_len, &out_tcp) == TCP_OK);
    printk("ack-only flags: %d\n", out_tcp.flags);
    printk("ack-only ackno: %u\n", out_tcp.ackno);
    printk("ack-only seqno: %u\n", out_tcp.seqno);
    assert(out_tcp.flags == TCP_ACK);
    assert(out_tcp.ackno == 9005);
    assert(out_tcp.seqno == 5001);

    tcp_set_output_fn(0);
    printk("TCP IP INTEGRATION TEST DONE\n");
    clean_reboot();
}
