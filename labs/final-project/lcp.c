#include "lcp.h"

enum {
    LCP_HDR_LEN = 4,
    LCP_MAX_DATA_LEN = 256,
};
static uint8_t g_our_last_confreq_id = 0;
static int g_our_confreq_acked = 0;
static int g_peer_confreq_acked = 0;
static lcp_state_t g_lcp_state = LCP_STATE_CLOSED;


static lcp_config_t g_lcp_cfg = {
    .wanted_mru = 1500,
    .wanted_accm = 0xFFFFFFFF,
    .magic_number = 0x12345678,
};

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

void lcp_init(const lcp_config_t *cfg) {
    if (cfg)
        g_lcp_cfg = *cfg;
}

int lcp_parse(const uint8_t *info, unsigned info_len, lcp_pkt_t *pkt) {
    if (!info || !pkt)
        return LCP_ERR;

    if (info_len < LCP_HDR_LEN)
        return LCP_BAD_LEN;

    pkt->code = info[0];
    pkt->identifier = info[1];
    pkt->length = get_be16(&info[2]);

    if (pkt->length < LCP_HDR_LEN)
        return LCP_BAD_LEN;

    if (pkt->length > info_len)
        return LCP_BAD_LEN;
    // first 4 bits are header, so we skip them
    pkt->data = &info[4];
    pkt->data_len = pkt->length - LCP_HDR_LEN;

    return LCP_OK;
}

void lcp_print_packet(const lcp_pkt_t *pkt) {
    if (!pkt) {
        printk("LCP: <null>\n");
        return;
    }

    printk("LCP: code=%d id=%d len=%d data_len=%d\n",
           pkt->code, pkt->identifier, pkt->length, pkt->data_len);
}

static int lcp_send_control(uint8_t code, uint8_t identifier,
                            const uint8_t *data, unsigned data_len) {
    uint8_t buf[LCP_HDR_LEN + LCP_MAX_DATA_LEN];
    unsigned total_len;

    if (data_len > LCP_MAX_DATA_LEN)
        return LCP_TOO_LARGE;

    total_len = LCP_HDR_LEN + data_len;

    buf[0] = code;
    buf[1] = identifier;
    put_be16(&buf[2], total_len);

    for (unsigned i = 0; i < data_len; i++)
        buf[4 + i] = data[i];

    return ppp_send(PPP_PROTO_LCP, buf, total_len);
}

static int lcp_send_config_ack(uint8_t identifier,
                               const uint8_t *req_data, unsigned req_len) {
    return lcp_send_control(LCP_CONFIG_ACK, identifier, req_data, req_len);
}

static int lcp_send_config_nak(uint8_t identifier,
                               const uint8_t *nak_data, unsigned nak_len) {
    return lcp_send_control(LCP_CONFIG_NAK, identifier, nak_data, nak_len);
}

static int lcp_send_config_rej(uint8_t identifier,
                               const uint8_t *rej_data, unsigned rej_len) {
    return lcp_send_control(LCP_CONFIG_REJ, identifier, rej_data, rej_len);
}

static int lcp_send_echo_reply(uint8_t identifier,
                               const uint8_t *req_data, unsigned req_len) {
    return lcp_send_control(LCP_ECHO_REPLY, identifier, req_data, req_len);
}

static int lcp_send_term_ack(uint8_t identifier,
                             const uint8_t *req_data, unsigned req_len) {
    return lcp_send_control(LCP_TERM_ACK, identifier, req_data, req_len);
}

static void lcp_update_state(void) {
    if (g_our_confreq_acked && g_peer_confreq_acked) {
        g_lcp_state = LCP_STATE_OPENED;
    } else if (g_our_confreq_acked) { // our config request was acked by the peer
        g_lcp_state = LCP_STATE_ACK_RCVD;
    } else if (g_peer_confreq_acked) {// peer config request was acked by us
        g_lcp_state = LCP_STATE_ACK_SENT;
    } else if (g_our_last_confreq_id != 0) {
        g_lcp_state = LCP_STATE_REQ_SENT;
    } else {
        g_lcp_state = LCP_STATE_CLOSED;
    }
}

// int lcp_send_config_req(uint8_t identifier) {
//     uint8_t req_data[6];

//     /*
//      * LCP option format:
//      *   Type (1)
//      *   Length (1)
//      *   Data (...)
//      *
//      * Magic-Number option:
//      *   type = 5
//      *   len  = 6
//      *   value = 4 bytes
//      */

//     req_data[0] = LCP_OPT_MAGIC_NUMBER;
//     req_data[1] = 6;
//     put_be32(&req_data[2], g_lcp_cfg.magic_number);

//     printk("LCP: sending our Configure-Request (id=%d, magic=%x)\n",
//            identifier, g_lcp_cfg.magic_number);

//     return lcp_send_control(LCP_CONFIG_REQ, identifier, req_data, sizeof req_data);
// }
int lcp_send_config_req(uint8_t identifier) {
    uint8_t req_data[6];

    g_our_last_confreq_id = identifier;
    g_our_confreq_acked = 0;

    lcp_update_state();

    req_data[0] = LCP_OPT_MAGIC_NUMBER;
    req_data[1] = 6;
    put_be32(&req_data[2], g_lcp_cfg.magic_number);

    printk("LCP: sending our Configure-Request (id=%d, magic=%x)\n",
           identifier, g_lcp_cfg.magic_number);

    return lcp_send_control(LCP_CONFIG_REQ, identifier, req_data, sizeof req_data);
}
static void lcp_reset_state(void) {
    g_our_last_confreq_id = 0;
    g_our_confreq_acked = 0;
    g_peer_confreq_acked = 0;
    g_lcp_state = LCP_STATE_CLOSED;
}
/*
 * Parse Configure-Request options and decide whether to:
 *   - ACK: all options acceptable
 *   - NAK: supported option, but we want a different value
 *   - REJ: unsupported option
 *
 * Rule we use here:
 *   - If any unsupported option appears -> send Configure-Reject
 *   - Else if any supported-but-wrong-value appears -> send Configure-Nak
 *   - Else -> send Configure-Ack
 */
static int lcp_handle_config_req(const lcp_pkt_t *pkt) {
    g_peer_confreq_acked = 0;
    lcp_update_state();
    uint8_t rej_buf[LCP_MAX_DATA_LEN];
    uint8_t nak_buf[LCP_MAX_DATA_LEN];
    unsigned rej_len = 0;
    unsigned nak_len = 0;

    const uint8_t *p = pkt->data;
    // data looks like Option-Type | Option-Length | Option-Data...
    // can have multiple options in the data
    unsigned rem = pkt->data_len;

    while (rem > 0) {
        uint8_t opt_type, opt_len;

        if (rem < 2) {
            printk("LCP: malformed option header\n");
            return LCP_BAD_PKT;
        }

        opt_type = p[0]; // like MRU, ACCM, MAGIC_NUMBER
        opt_len = p[1]; // length of the option

        if (opt_len < 2 || opt_len > rem) {
            printk("LCP: malformed option len=%d rem=%d\n", opt_len, rem);
            return LCP_BAD_PKT;
        }

        switch (opt_type) {
        case LCP_OPT_MRU:
            /*
             * MRU option format:
             *   type=1 len=4 value(2 bytes)
             */
            if (opt_len != 4) {
                /* bad format -> reject whole option as received */
                for (unsigned i = 0; i < opt_len; i++)
                    rej_buf[rej_len + i] = p[i];
                rej_len += opt_len;
            } else {
                uint16_t peer_mru = get_be16(&p[2]);
                printk("LCP: peer MRU=%d\n", peer_mru);

                if (peer_mru != g_lcp_cfg.wanted_mru) {
                    /* supported option, but propose our preferred MRU */
                    nak_buf[nak_len + 0] = LCP_OPT_MRU; // type of option we are sending
                    nak_buf[nak_len + 1] = 4; // length of what we are sending (1 byte)
                    put_be16(&nak_buf[nak_len + 2], g_lcp_cfg.wanted_mru);
                    nak_len += 4;
                }
            }
            break;

        case LCP_OPT_ACCM:
            /*
             * ACCM option format:
             *   type=2 len=6 value(4 bytes)
             */
            if (opt_len != 6) {
                for (unsigned i = 0; i < opt_len; i++)
                    rej_buf[rej_len + i] = p[i];
                rej_len += opt_len;
            } else {
                uint32_t peer_accm = get_be32(&p[2]);
                printk("LCP: peer ACCM=%x\n", peer_accm);

                if (peer_accm != g_lcp_cfg.wanted_accm) {
                    nak_buf[nak_len + 0] = LCP_OPT_ACCM;
                    nak_buf[nak_len + 1] = 6;
                    put_be32(&nak_buf[nak_len + 2], g_lcp_cfg.wanted_accm);
                    nak_len += 6;
                }
            }
            break;

        case LCP_OPT_MAGIC_NUMBER:
            /*
             * Magic Number format:
             *   type=5 len=6 value(4 bytes)
             */
            if (opt_len != 6) {
                for (unsigned i = 0; i < opt_len; i++)
                    rej_buf[rej_len + i] = p[i];
                rej_len += opt_len;
            } else {
                uint32_t peer_magic = get_be32(&p[2]);
                printk("LCP: peer magic=%x\n", peer_magic);

                /*
                 * Minimal implementation:
                 * accept any peer magic number.
                 * A more complete version would detect loopback collisions.
                 */
                (void)peer_magic;
            }
            break;

        default:
            printk("LCP: rejecting unsupported option type=%d len=%d\n",
                   opt_type, opt_len);
            for (unsigned i = 0; i < opt_len; i++)
                rej_buf[rej_len + i] = p[i];
            rej_len += opt_len;
            break;
        }

        p += opt_len;
        rem -= opt_len;
    }

    if (rej_len > 0) {
        printk("LCP: sending Configure-Reject\n");
        return lcp_send_config_rej(pkt->identifier, rej_buf, rej_len);
    }

    if (nak_len > 0) {
        printk("LCP: sending Configure-Nak\n");
        return lcp_send_config_nak(pkt->identifier, nak_buf, nak_len);
    }

    printk("LCP: sending Configure-Ack\n");
    g_peer_confreq_acked = 1;
    lcp_update_state();
    return lcp_send_config_ack(pkt->identifier, pkt->data, pkt->data_len);
}
/* Useful for PPP to know if IPCP should start sending IP packets */
int lcp_is_open(void) {
    return g_lcp_state == LCP_STATE_OPENED;
}

int lcp_handle_packet(const uint8_t *info, unsigned info_len) {
    lcp_pkt_t pkt;
    int ret = lcp_parse(info, info_len, &pkt);
    if (ret < 0) {
        printk("LCP: parse failed %d\n", ret);
        return ret;
    }
    int r;

    lcp_print_packet(&pkt);

    switch (pkt.code) {
    case LCP_CONFIG_REQ:
        return lcp_handle_config_req(&pkt);

    case LCP_CONFIG_ACK:
        printk("LCP: received Configure-Ack\n");
        if (pkt.identifier == g_our_last_confreq_id) {
            g_our_confreq_acked = 1;
            printk("LCP: our Configure-Request was acked\n");
            lcp_update_state();
        }
        return LCP_OK;

    case LCP_CONFIG_NAK:
        printk("LCP: received Configure-Nak\n");
        return LCP_OK;

    case LCP_CONFIG_REJ:
        printk("LCP: received Configure-Reject\n");
        return LCP_OK;

    case LCP_TERM_REQ:
        // printk("LCP: received Terminate-Request, sending Terminate-Ack\n");
        // lcp_reset_state();
        // return lcp_send_term_ack(pkt.identifier, pkt.data, pkt.data_len);
        r = lcp_send_term_ack(pkt.identifier, pkt.data, pkt.data_len);
        lcp_reset_state();
        return r;

    case LCP_TERM_ACK:
        printk("LCP: received Terminate-Ack\n");
        lcp_reset_state();
        return LCP_OK;

    case LCP_ECHO_REQ:
        printk("LCP: received Echo-Request, sending Echo-Reply\n");
        return lcp_send_echo_reply(pkt.identifier, pkt.data, pkt.data_len);

    case LCP_ECHO_REPLY:
        printk("LCP: received Echo-Reply\n");
        return LCP_OK;

    default:
        printk("LCP: unsupported code=%d\n", pkt.code);
        return LCP_OK;
    }
}