#include "ppp.h"

/* PPP header is: address(1) + control(1) + protocol(2) */
enum {
    PPP_HDR_LEN = 4
};

static uint16_t ppp_get_be16(const uint8_t *p) {
    return ((uint16_t)p[0] << 8) | p[1];
}

static void ppp_put_be16(uint8_t *p, uint16_t x) {
    p[0] = (x >> 8) & 0xFF;
    p[1] = x & 0xFF;
}

/*
 * Parse a decoded PPP frame payload:
 *   FF 03 proto_hi proto_lo info...
 *
 * On success:
 *   - pkt->info points into buf
 *   - caller must keep buf alive while using pkt->info
 */
int ppp_parse(const uint8_t *buf, unsigned len, ppp_pkt_t *pkt) {
    if (!buf || !pkt)
        return PPP_ERROR;

    if (len < PPP_HDR_LEN)
        return PPP_BAD_FRAME;

    pkt->address  = buf[0];
    pkt->control  = buf[1];
    pkt->protocol = ppp_get_be16(&buf[2]);
    pkt->info     = &buf[4];
    pkt->info_len = len - PPP_HDR_LEN;

    if (pkt->address != PPP_ADDRESS)
        return PPP_BAD_FRAME;

    if (pkt->control != PPP_CONTROL)
        return PPP_BAD_FRAME;

    return PPP_OK;
}

/*
 * Build a PPP packet into out[] but do not HDLC-frame it yet.
 * Output format:
 *   FF 03 proto_hi proto_lo info...
 */
int ppp_build(uint16_t protocol,
              const uint8_t *info, unsigned info_len,
              uint8_t *out, unsigned out_size) {
    if (!out)
        return PPP_ERROR;

    if (info_len > 0 && !info)
        return PPP_ERROR;

    if (PPP_HDR_LEN + info_len > out_size)
        return PPP_TOO_LARGE;

    out[0] = PPP_ADDRESS;
    out[1] = PPP_CONTROL;
    ppp_put_be16(&out[2], protocol);

    for (unsigned i = 0; i < info_len; i++)
        out[4 + i] = info[i];

    return PPP_HDR_LEN + info_len;
}

/*
 * Build PPP payload and hand it to HDLC layer for framing + UART send.
 */
int ppp_send(uint16_t protocol, const uint8_t *info, unsigned info_len) {
    uint8_t pkt[PPP_HDR_LEN + PPP_MAX_INFO_LEN];

    if (info_len > PPP_MAX_INFO_LEN)
        return PPP_TOO_LARGE;

    int pkt_len = ppp_build(protocol, info, info_len, pkt, sizeof pkt);
    if (pkt_len < 0)
        return pkt_len;

    return hdlc_send(pkt, (unsigned)pkt_len);
}

/*
 * Receive one HDLC frame, then parse the PPP header.
 *
 * buf is caller-provided storage for the decoded PPP bytes.
 * pkt->info will point inside buf.
 */
int ppp_recv(ppp_pkt_t *pkt, uint8_t *buf, unsigned buf_size) {
    if (!pkt || !buf)
        return PPP_ERROR;

    int n = hdlc_recv(buf, buf_size);
    if (n < 0)
        return n;

    return ppp_parse(buf, (unsigned)n, pkt);
}

/*
 * Simple protocol demux:
 *   LCP  -> lcp_handler
 *   IPCP -> ipcp_handler
 *   IP   -> ip_handler
 */
int ppp_dispatch(const ppp_pkt_t *pkt,
                 ppp_protocol_handler_t lcp_handler,
                 ppp_protocol_handler_t ipcp_handler,
                 ppp_protocol_handler_t ip_handler) {
    if (!pkt)
        return PPP_ERROR;

    switch (pkt->protocol) {
    case PPP_PROTO_LCP:
        if (!lcp_handler)
            return PPP_UNSUPPORTED;
        return lcp_handler(pkt->info, pkt->info_len);

    case PPP_PROTO_IPCP:
        if (!ipcp_handler)
            return PPP_UNSUPPORTED;
        return ipcp_handler(pkt->info, pkt->info_len);

    case PPP_PROTO_IP:
        if (!ip_handler)
            return PPP_UNSUPPORTED;
        return ip_handler(pkt->info, pkt->info_len);

    default:
        printk("ppp_dispatch: unsupported protocol 0x%x\n", pkt->protocol);
        return PPP_UNSUPPORTED;
    }
}

void ppp_print_packet(const ppp_pkt_t *pkt) {
    if (!pkt) {
        printk("PPP: <null>\n");
        return;
    }

    printk("PPP: addr=0x%x ctrl=0x%x proto=0x%x info_len=%d\n",
           pkt->address, pkt->control, pkt->protocol, pkt->info_len);
}