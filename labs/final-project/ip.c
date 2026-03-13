#include "ip.h"
#include "ipcp.h"

static uint16_t get_be16(const uint8_t *p) {
    return ((uint16_t)p[0] << 8) | p[1];
}

static uint32_t get_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  |
           ((uint32_t)p[3]);
}

static void put_be16(uint8_t *p, uint16_t x) {
    p[0] = (x >> 8) & 0xFF;
    p[1] = x & 0xFF;
}

static void put_be32(uint8_t *p, uint32_t x) {
    p[0] = (x >> 24) & 0xFF;
    p[1] = (x >> 16) & 0xFF;
    p[2] = (x >> 8) & 0xFF;
    p[3] = x & 0xFF;
}

// ones' complement checksum (RFC 791 section 3.1).
// works for ip header, icmp, udp pseudo-header.
uint16_t ip_checksum(const uint8_t *data, unsigned len) {
    uint32_t sum = 0;

    // sum 16-bit words
    for (unsigned i = 0; i + 1 < len; i += 2)
        sum += ((uint32_t)data[i] << 8) | data[i + 1];

    // odd trailing byte
    if (len & 1)
        sum += (uint32_t)data[len - 1] << 8;

    // fold carries
    while (sum >> 16)
        sum = (sum >> 16) + (sum & 0xFFFF);

    return (uint16_t)(~sum);
}

// parse raw ip packet bytes into hdr struct.
// validates version, ihl, length, and checksum.
int ip_parse(const uint8_t *pkt, unsigned pkt_len, ip_hdr_t *hdr) {
    if (!pkt || !hdr)
        return IP_ERR;

    if (pkt_len < IP_HDR_LEN)
        return IP_TOO_SHORT;

    hdr->version = (pkt[0] >> 4) & 0x0F;
    hdr->ihl = pkt[0] & 0x0F;

    if (hdr->version != 4)
        return IP_BAD_HDR;

    if (hdr->ihl < 5)
        return IP_BAD_HDR;

    unsigned hdr_len = hdr->ihl * 4;
    if (pkt_len < hdr_len)
        return IP_TOO_SHORT;

    hdr->tos = pkt[1];
    hdr->total_len = get_be16(&pkt[2]);
    hdr->id = get_be16(&pkt[4]);

    uint16_t flags_offset = get_be16(&pkt[6]);
    hdr->flags = (flags_offset >> 13) & 0x07;
    hdr->frag_offset = flags_offset & 0x1FFF;

    hdr->ttl = pkt[8];
    hdr->protocol = pkt[9];
    hdr->checksum = get_be16(&pkt[10]);
    hdr->src = get_be32(&pkt[12]);
    hdr->dst = get_be32(&pkt[16]);

    // verify checksum over the header only
    if (ip_checksum(pkt, hdr_len) != 0)
        return IP_BAD_CKSUM;

    if (hdr->total_len < hdr_len)
        return IP_BAD_HDR;

    if (hdr->total_len > pkt_len)
        return IP_TOO_SHORT;

    hdr->payload = &pkt[hdr_len];
    hdr->payload_len = hdr->total_len - hdr_len;

    return IP_OK;
}

// build a raw ip packet into out[].
// fills in version, ihl, total_len, and computes checksum.
// hdr->src, hdr->dst, hdr->protocol, hdr->ttl should be set by caller.
int ip_build(const ip_hdr_t *hdr, const uint8_t *payload, unsigned payload_len,
             uint8_t *out, unsigned out_size) {
    unsigned total_len = IP_HDR_LEN + payload_len;

    if (!hdr || !out)
        return IP_ERR;
    if (total_len > out_size)
        return IP_ERR;

    // version=4, ihl=5 (no options)
    out[0] = 0x45;
    out[1] = hdr->tos;
    put_be16(&out[2], total_len);
    put_be16(&out[4], hdr->id);

    uint16_t flags_offset = ((uint16_t)(hdr->flags & 0x07) << 13) |
                            (hdr->frag_offset & 0x1FFF);
    put_be16(&out[6], flags_offset);

    out[8] = hdr->ttl;
    out[9] = hdr->protocol;
    // zero checksum for computation
    out[10] = 0;
    out[11] = 0;
    put_be32(&out[12], hdr->src);
    put_be32(&out[16], hdr->dst);

    // compute header checksum
    uint16_t cksum = ip_checksum(out, IP_HDR_LEN);
    put_be16(&out[10], cksum);

    // copy payload
    for (unsigned i = 0; i < payload_len; i++)
        out[IP_HDR_LEN + i] = payload[i];

    return (int)total_len;
}

// send an ip packet over ppp.
// builds header with given src/dst/protocol, then calls ppp_send.
int ip_send(uint32_t src, uint32_t dst, uint8_t protocol,
            const uint8_t *payload, unsigned payload_len) {
    uint8_t pkt[IP_HDR_LEN + 1500];

    if (payload_len > 1500)
        return IP_ERR;

    ip_hdr_t hdr = {
        .version = 4,
        .ihl = 5,
        .tos = 0,
        .total_len = 0, // filled by ip_build
        .id = 0,
        .flags = 0x02,  // DF bit
        .frag_offset = 0,
        .ttl = 64,
        .protocol = protocol,
        .checksum = 0,
        .src = src,
        .dst = dst,
        .payload = 0,
        .payload_len = 0,
    };

    int total = ip_build(&hdr, payload, payload_len, pkt, sizeof pkt);
    if (total < 0)
        return total;

    return ppp_send(PPP_PROTO_IP, pkt, (unsigned)total);
}

void ip_print_hdr(const ip_hdr_t *hdr) {
    if (!hdr) {
        printk("IP: <null>\n");
        return;
    }

    printk("IP: ver=%d ihl=%d len=%d proto=%d ttl=%d src=%d.%d.%d.%d dst=%d.%d.%d.%d\n",
           hdr->version, hdr->ihl, hdr->total_len,
           hdr->protocol, hdr->ttl,
           (hdr->src >> 24) & 0xFF, (hdr->src >> 16) & 0xFF,
           (hdr->src >> 8) & 0xFF, hdr->src & 0xFF,
           (hdr->dst >> 24) & 0xFF, (hdr->dst >> 16) & 0xFF,
           (hdr->dst >> 8) & 0xFF, hdr->dst & 0xFF);
}

// handle an incoming ip packet from ppp_dispatch
// TODO for now just parses and prints, will add icmp/udp handlers later
int ip_handle_packet(const uint8_t *info, unsigned info_len) {
    ip_hdr_t hdr;
    int r = ip_parse(info, info_len, &hdr);
    if (r < 0) {
        printk("IP: parse failed %d\n", r);
        return r;
    }

    ip_print_hdr(&hdr);

    switch (hdr.protocol) {
    case IP_PROTO_ICMP:
        printk("IP: received ICMP packet (%d bytes)\n", hdr.payload_len);
        if (hdr.payload_len < 8) {
            return IP_TOO_SHORT;
        }
        // TODO: icmp handler
        struct icmp_echo *icmp = (struct icmp_echo *)hdr.payload;
        // ignore Echo Reply for now
        if (icmp->type != 8) {
            printk("IP: received ICMP echo reply\n");
            return IP_OK;
        }
        // when Echo Request, build reply payload 
        uint8_t out_icmp[1500];
        if (hdr.payload_len > sizeof out_icmp)
            return IP_ERR;
        
        const uint8_t *in = hdr.payload;
        for (unsigned i = 0; i < hdr.payload_len; i++)
            out_icmp[i] = in[i];
        out_icmp[0] = 0;   // Echo Reply
        out_icmp[1] = 0;   // code
        out_icmp[2] = 0;   // checksum high
        out_icmp[3] = 0;   // checksum low

        uint16_t cksum = ip_checksum(out_icmp, hdr.payload_len);
        out_icmp[2] = (cksum >> 8) & 0xff;
        out_icmp[3] = cksum & 0xff;
        // swap source and destination IP
        return ip_send(hdr.dst, hdr.src, IP_PROTO_ICMP, out_icmp, hdr.payload_len);

    case IP_PROTO_UDP:
        printk("IP: received UDP packet (%d bytes)\n", hdr.payload_len);
        // TODO: udp handler
        return IP_OK;

    default:
        printk("IP: unsupported protocol %d\n", hdr.protocol);
        return IP_OK;
    }
}
