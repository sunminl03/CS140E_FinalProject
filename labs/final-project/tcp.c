#include "tcp.h"
#include "ip.h"
#include "net_util.h"

int tcp_parse(const uint8_t *seg, unsigned seg_len, tcp_hdr_t *hdr) {
    if (!seg || !hdr)
        return TCP_ERR;

    if (seg_len < TCP_HDR_LEN)
        return TCP_TOO_SHORT;

    hdr->src_port = get_be16(&seg[0]);
    hdr->dst_port = get_be16(&seg[2]);
    hdr->seqno = get_be32(&seg[4]);
    hdr->ackno = get_be32(&seg[8]);
    hdr->data_offset = (seg[12] >> 4) & 0xF; // data field is only 4 bits
    hdr->flags = seg[13];
    hdr->window = get_be16(&seg[14]);
    hdr->checksum = get_be16(&seg[16]);
    hdr->urgent_ptr = get_be16(&seg[18]);

    if (hdr->data_offset < 5)
        return TCP_BAD_HDR;

    unsigned hdr_len = hdr->data_offset * 4;
    if (hdr_len > TCP_MAX_HDR_LEN)
        return TCP_BAD_HDR;

    if (seg_len < hdr_len)
        return TCP_TOO_SHORT;

    hdr->options_len = hdr_len - TCP_HDR_LEN;
    hdr->options = hdr->options_len ? &seg[TCP_HDR_LEN] : 0;
    hdr->payload = &seg[hdr_len];
    hdr->payload_len = seg_len - hdr_len;

    return TCP_OK;
}

int tcp_build(const tcp_hdr_t *hdr, const uint8_t *payload, unsigned payload_len,
              uint8_t *out, unsigned out_size) {
    if (!hdr || !out)
        return TCP_ERR;

    if (payload_len > 0 && !payload)
        return TCP_ERR;

    if (hdr->options_len % 4 != 0)
        return TCP_BAD_HDR;

    unsigned hdr_len = TCP_HDR_LEN + hdr->options_len;
    if (hdr_len > TCP_MAX_HDR_LEN)
        return TCP_BAD_HDR;

    if (hdr->data_offset && hdr->data_offset != hdr_len / 4)
        return TCP_BAD_HDR;

    unsigned total_len = hdr_len + payload_len;
    if (total_len > out_size)
        return TCP_ERR;

    put_be16(&out[0], hdr->src_port);
    put_be16(&out[2], hdr->dst_port);
    put_be32(&out[4], hdr->seqno);
    put_be32(&out[8], hdr->ackno);
    out[12] = (uint8_t)((hdr_len / 4) << 4);
    out[13] = hdr->flags;
    put_be16(&out[14], hdr->window);
    put_be16(&out[16], hdr->checksum);
    put_be16(&out[18], hdr->urgent_ptr);

    for (unsigned i = 0; i < hdr->options_len; i++)
        out[TCP_HDR_LEN + i] = hdr->options[i];

    for (unsigned i = 0; i < payload_len; i++)
        out[hdr_len + i] = payload[i];

    return total_len;
}

uint16_t tcp_checksum(uint32_t src_ip, uint32_t dst_ip,
                      const uint8_t *tcp_seg, unsigned tcp_len) {
    uint8_t buf[12 + 1500];

    if (!tcp_seg || tcp_len > 1500)
        return 0;

    put_be32(&buf[0], src_ip);
    put_be32(&buf[4], dst_ip);
    buf[8] = 0;
    buf[9] = IP_PROTO_TCP;
    put_be16(&buf[10], tcp_len);

    for (unsigned i = 0; i < tcp_len; i++)
        buf[12 + i] = tcp_seg[i];

    return ip_checksum(buf, 12 + tcp_len);
}
