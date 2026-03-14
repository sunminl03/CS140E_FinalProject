#include "udp.h"
#include "ip.h"
#include "net_util.h"

int udp_parse(const uint8_t *pkt, unsigned pkt_len, udp_hdr_t *hdr) {
    if (!pkt || !hdr)
        return UDP_ERR;

    if (pkt_len < UDP_HDR_LEN)
        return UDP_WRONG_SIZE;

    hdr->src_port = get_be16(&pkt[0]);
    hdr->dst_port = get_be16(&pkt[2]);
    hdr->len = get_be16(&pkt[4]);
    hdr->checksum = get_be16(&pkt[6]);

    if (hdr->len < UDP_HDR_LEN)
        return UDP_BAD_HDR;

    if (hdr->len > pkt_len)
        return UDP_WRONG_SIZE;

    hdr->payload = &pkt[UDP_HDR_LEN];
    hdr->payload_len = hdr->len - UDP_HDR_LEN;

    return UDP_OK;
}

uint16_t udp_checksum(uint32_t src_ip, uint32_t dst_ip,
                      const uint8_t *udp_pkt, unsigned udp_len) {
    uint8_t buf[12 + 1500];

    if (!udp_pkt || udp_len > 1500)
        return 0;

    put_be32(&buf[0], src_ip);
    put_be32(&buf[4], dst_ip);
    buf[8] = 0;
    buf[9] = IP_PROTO_UDP;
    put_be16(&buf[10], udp_len);

    for (unsigned i = 0; i < udp_len; i++)
        buf[12 + i] = udp_pkt[i];

    return ip_checksum(buf, 12 + udp_len);
}

int udp_handle_packet(uint32_t src_ip, uint32_t dst_ip,
                      const uint8_t *pkt, unsigned pkt_len) {
    udp_hdr_t hdr;
    int r = udp_parse(pkt, pkt_len, &hdr);
    if (r < 0)
        return r;

    if (hdr.checksum != 0 && udp_checksum(src_ip, dst_ip, pkt, hdr.len) != 0)
        return UDP_BAD_CKSUM;

    if (hdr.dst_port != UDP_ECHO_PORT)
        return UDP_OK;

    uint8_t out_udp[1500];
    if (hdr.len > sizeof out_udp)
        return UDP_ERR;

    put_be16(&out_udp[0], hdr.dst_port);
    put_be16(&out_udp[2], hdr.src_port);
    put_be16(&out_udp[4], hdr.len);
    out_udp[6] = 0;
    out_udp[7] = 0;

    for (unsigned i = 0; i < hdr.payload_len; i++)
        out_udp[UDP_HDR_LEN + i] = hdr.payload[i];

    uint16_t cksum = udp_checksum(dst_ip, src_ip, out_udp, hdr.len);
    if (cksum == 0) // send 0xffff instead of empty cksum
        cksum = 0xFFFF;
    put_be16(&out_udp[6], cksum);

    return ip_send(dst_ip, src_ip, IP_PROTO_UDP, out_udp, hdr.len);
}
