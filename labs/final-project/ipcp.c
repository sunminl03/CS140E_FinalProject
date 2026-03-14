#include "ipcp.h"
#include "lcp.h"
#include "net_util.h"

enum {
    IPCP_HDR_LEN = 4,
    IPCP_MAX_DATA_LEN = 256,
};

/* next id to use for our outgoing ConfReq */
static uint8_t g_next_confreq_id = 1;
/* id of our currently outstanding ConfReq */
static uint8_t g_our_last_confreq_id = 0;
/* whether our latest ConfReq was ACKed by peer */
static int g_our_confreq_acked = 0;
/* whether we ACKed peer's latest ConfReq */
static int g_peer_confreq_acked = 0;
/* current IPCP state */
static ipcp_state_t g_ipcp_state = IPCP_STATE_CLOSED;

/* minimal IPCP config */
static ipcp_config_t g_ipcp_cfg = {
    .want_ip_address = 1,
    .our_ip_address = 0x00000000,   /* 0.0.0.0 */
};

/* peer's IP address learned from peer's Configure-Request */
static uint32_t g_peer_ip_address = 0x00000000;

/*
Returns next nonzero Configure-Request ID for retries.
*/
static uint8_t ipcp_next_id(void) {
    uint8_t id = g_next_confreq_id++;
    if (id == 0) {
        id = g_next_confreq_id++;
        if (id == 0)
            id = 1;
    }
    return id;
}
/*
Resets IPCP negotiation state, IDs, ack flags, and cached peer IP.
*/
static void ipcp_reset_state(void) {
    g_next_confreq_id = 1;
    g_our_last_confreq_id = 0;
    g_our_confreq_acked = 0;
    g_peer_confreq_acked = 0;
    g_ipcp_state = IPCP_STATE_CLOSED;
    g_peer_ip_address = 0x00000000;
}
/*
Computes state (CLOSED/REQ_SENT/ACK_RCVD/ACK_SENT/OPENED) from ack flags.
*/
static void ipcp_update_state(void) {
    if (g_our_confreq_acked && g_peer_confreq_acked) {
        g_ipcp_state = IPCP_STATE_OPENED;
    } else if (g_our_confreq_acked) {
        g_ipcp_state = IPCP_STATE_ACK_RCVD;
    } else if (g_peer_confreq_acked) {
        g_ipcp_state = IPCP_STATE_ACK_SENT;
    } else if (g_our_last_confreq_id != 0) {
        g_ipcp_state = IPCP_STATE_REQ_SENT;
    } else {
        g_ipcp_state = IPCP_STATE_CLOSED;
    }
}
/*
Loads config (want_ip_address, our_ip_address) and resets runtime state.
*/
void ipcp_init(const ipcp_config_t *cfg) {
    if (cfg)
        g_ipcp_cfg = *cfg;
    ipcp_reset_state();
}

/*
Parses IPCP header (code/id/len), validates length, sets pkt->data and data_len.
*/
int ipcp_parse(const uint8_t *info, unsigned info_len, ipcp_pkt_t *pkt) {
    if (!info || !pkt)
        return IPCP_ERR;

    if (info_len < IPCP_HDR_LEN)
        return IPCP_BAD_LEN;

    pkt->code = info[0];
    pkt->identifier = info[1];
    pkt->length = get_be16(&info[2]);

    if (pkt->length < IPCP_HDR_LEN)
        return IPCP_BAD_LEN;

    if (pkt->length > info_len)
        return IPCP_BAD_LEN;

    pkt->data = &info[4];
    pkt->data_len = pkt->length - IPCP_HDR_LEN;
    return IPCP_OK;
}

void ipcp_print_packet(const ipcp_pkt_t *pkt) {
    if (!pkt) {
        printk("IPCP: <null>\n");
        return;
    }

    printk("IPCP: code=%d id=%d len=%d data_len=%d\n",
           pkt->code, pkt->identifier, pkt->length, pkt->data_len);
}

/*
Generic sender for IPCP control packets (builds header + payload, calls ppp_send with PPP_PROTO_IPCP).
*/
static int ipcp_send_control(uint8_t code, uint8_t identifier,
                             const uint8_t *data, unsigned data_len) {
    uint8_t buf[IPCP_HDR_LEN + IPCP_MAX_DATA_LEN];
    unsigned total_len;

    if (data_len > IPCP_MAX_DATA_LEN)
        return IPCP_TOO_LARGE;

    total_len = IPCP_HDR_LEN + data_len;

    buf[0] = code;
    buf[1] = identifier;
    put_be16(&buf[2], total_len);

    for (unsigned i = 0; i < data_len; i++)
        buf[4 + i] = data[i];

    return ppp_send(PPP_PROTO_IPCP, buf, total_len);
}

static int ipcp_send_config_ack(uint8_t identifier,
                                const uint8_t *req_data, unsigned req_len) {
    return ipcp_send_control(IPCP_CONFIG_ACK, identifier, req_data, req_len);
}

static int ipcp_send_config_nak(uint8_t identifier,
                                const uint8_t *nak_data, unsigned nak_len) {
    return ipcp_send_control(IPCP_CONFIG_NAK, identifier, nak_data, nak_len);
}

static int ipcp_send_config_rej(uint8_t identifier,
                                const uint8_t *rej_data, unsigned rej_len) {
    return ipcp_send_control(IPCP_CONFIG_REJ, identifier, rej_data, rej_len);
}

static int ipcp_send_term_ack(uint8_t identifier,
                              const uint8_t *req_data, unsigned req_len) {
    return ipcp_send_control(IPCP_TERM_ACK, identifier, req_data, req_len);
}

/*
Builds/sends our IPCP Configure-Request (currently IP-Address option if enabled), updates request ID and state.
Refuses to run unless LCP is open.
*/
int ipcp_send_config_req(uint8_t identifier) {
    uint8_t req_data[IPCP_MAX_DATA_LEN];
    unsigned len = 0;

    if (!lcp_is_open())
        return IPCP_NOT_READY;

    g_our_last_confreq_id = identifier;
    g_our_confreq_acked = 0;
    g_next_confreq_id = identifier + 1;
    if (g_next_confreq_id == 0)
        g_next_confreq_id = 1;
    ipcp_update_state();

    if (g_ipcp_cfg.want_ip_address) {
        req_data[len + 0] = IPCP_OPT_IP_ADDRESS;
        req_data[len + 1] = 6;
        put_be32(&req_data[len + 2], g_ipcp_cfg.our_ip_address);
        len += 6;
    }

    printk("IPCP: sending our Configure-Request (id=%d, our_ip=0x%x)\n",
           identifier, g_ipcp_cfg.our_ip_address);

    return ipcp_send_control(IPCP_CONFIG_REQ, identifier, req_data, len);
}

/* *** It Seems like our pppd does not give us suggested address. It rejects when we send 0.0.0.0.
 * Peer NAKed our request. For minimal IPCP, the important case is:
 *   peer suggests an IP address for us in option 3.
 */
static int ipcp_handle_config_nak(const ipcp_pkt_t *pkt) {
    const uint8_t *p = pkt->data;
    unsigned rem = pkt->data_len;

    printk("IPCP: handling Configure-Nak for our request id=%d\n", pkt->identifier);

    if (pkt->identifier != g_our_last_confreq_id) {
        printk("IPCP: ignoring Configure-Nak for old id=%d (expected %d)\n",
               pkt->identifier, g_our_last_confreq_id);
        return IPCP_OK;
    }

    while (rem > 0) {
        uint8_t opt_type, opt_len;

        if (rem < 2) {
            printk("IPCP: malformed Configure-Nak option header\n");
            return IPCP_BAD_PKT;
        }

        opt_type = p[0];
        opt_len = p[1];

        if (opt_len < 2 || opt_len > rem) {
            printk("IPCP: malformed Configure-Nak option len=%d rem=%d\n",
                   opt_len, rem);
            return IPCP_BAD_PKT;
        }

        switch (opt_type) {
        case IPCP_OPT_IP_ADDRESS:
            if (opt_len != 6) {
                printk("IPCP: bad IP-Address option length in Configure-Nak\n");
                return IPCP_BAD_PKT;
            } else {
                uint32_t suggested_ip = get_be32(&p[2]);
                printk("IPCP: peer suggested our IP address = 0x%x\n", suggested_ip);
                g_ipcp_cfg.want_ip_address = 1;
                g_ipcp_cfg.our_ip_address = suggested_ip;
            }
            break;

        default:
            printk("IPCP: ignoring unknown option %d in Configure-Nak\n", opt_type);
            break;
        }

        p += opt_len;
        rem -= opt_len;
    }

    g_our_confreq_acked = 0;
    ipcp_update_state();
    return ipcp_send_config_req(ipcp_next_id());
}

/*
Handles peer REJ to our request
If ID matches, disables rejected options (e.g. IP-Address), updates state, optionally resends ConfReq.
*/
static int ipcp_handle_config_rej(const ipcp_pkt_t *pkt) {
    const uint8_t *p = pkt->data;
    unsigned rem = pkt->data_len;

    printk("IPCP: handling Configure-Reject for our request id=%d\n", pkt->identifier);

    if (pkt->identifier != g_our_last_confreq_id) {
        printk("IPCP: ignoring Configure-Reject for old id=%d (expected %d)\n",
               pkt->identifier, g_our_last_confreq_id);
        return IPCP_OK;
    }

    while (rem > 0) {
        uint8_t opt_type, opt_len;

        if (rem < 2) {
            printk("IPCP: malformed Configure-Reject option header\n");
            return IPCP_BAD_PKT;
        }

        opt_type = p[0];
        opt_len = p[1];

        if (opt_len < 2 || opt_len > rem) {
            printk("IPCP: malformed Configure-Reject option len=%d rem=%d\n",
                   opt_len, rem);
            return IPCP_BAD_PKT;
        }

        switch (opt_type) {
        case IPCP_OPT_IP_ADDRESS:
            printk("IPCP: peer rejected IP-Address option\n");
            g_ipcp_cfg.want_ip_address = 0;
            break;

        default:
            printk("IPCP: peer rejected unknown option type=%d\n", opt_type);
            break;
        }

        p += opt_len;
        rem -= opt_len;
    }

    g_our_confreq_acked = 0;
    ipcp_update_state();

    if (!g_ipcp_cfg.want_ip_address)
        return IPCP_OK;

    return ipcp_send_config_req(ipcp_next_id());
}

/*
Handles peer Configure-Request.
Parses peer options:
    IP-Address: accept/store nonzero, NAK if peer asks 0.0.0.0 (with suggested address).
    Compression/unknown options: reject.
Sends REJ if needed, else NAK if needed, else ACK.
Marks peer request acked on ACK path.
*/
static int ipcp_handle_config_req(const ipcp_pkt_t *pkt) {
    uint8_t rej_buf[IPCP_MAX_DATA_LEN];
    uint8_t nak_buf[IPCP_MAX_DATA_LEN];
    unsigned rej_len = 0;
    unsigned nak_len = 0;

    const uint8_t *p = pkt->data;
    unsigned rem = pkt->data_len;

    g_peer_confreq_acked = 0;
    ipcp_update_state();

    while (rem > 0) {
        uint8_t opt_type, opt_len;

        if (rem < 2) {
            printk("IPCP: malformed option header\n");
            return IPCP_BAD_PKT;
        }

        opt_type = p[0];
        opt_len = p[1];

        if (opt_len < 2 || opt_len > rem) {
            printk("IPCP: malformed option len=%d rem=%d\n", opt_len, rem);
            return IPCP_BAD_PKT;
        }

        switch (opt_type) {
        case IPCP_OPT_IP_ADDRESS:
            if (opt_len != 6) {
                for (unsigned i = 0; i < opt_len; i++)
                    rej_buf[rej_len + i] = p[i];
                rej_len += opt_len;
            } else {
                uint32_t peer_ip = get_be32(&p[2]);
                if (peer_ip == 0) {
                    /*
                     * Peer asks us to suggest an address.
                     * Use a simple static suggestion for now.
                     */
                    uint32_t suggested_peer_ip = 0x0A000001; // 10.0.0.1
                    nak_buf[nak_len + 0] = IPCP_OPT_IP_ADDRESS;
                    nak_buf[nak_len + 1] = 6;
                    put_be32(&nak_buf[nak_len + 2], suggested_peer_ip);
                    nak_len += 6;
                    printk("IPCP: peer requested 0.0.0.0, NAK with 0x%x\n",
                           suggested_peer_ip);
                } else {
                    g_peer_ip_address = peer_ip;
                    printk("IPCP: peer IP address = 0x%x\n", g_peer_ip_address);
                }
            }
            break;

        case IPCP_OPT_IP_COMPRESSION_PROTOCOL:
            printk("IPCP: rejecting compression option\n");
            for (unsigned i = 0; i < opt_len; i++)
                rej_buf[rej_len + i] = p[i];
            rej_len += opt_len;
            break;

        default:
            printk("IPCP: rejecting unsupported option type=%d len=%d\n",
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
        printk("IPCP: sending Configure-Reject\n");
        return ipcp_send_config_rej(pkt->identifier, rej_buf, rej_len);
    }

    if (nak_len > 0) {
        printk("IPCP: sending Configure-Nak\n");
        return ipcp_send_config_nak(pkt->identifier, nak_buf, nak_len);
    }

    printk("IPCP: sending Configure-Ack\n");
    g_peer_confreq_acked = 1;
    ipcp_update_state();
    return ipcp_send_config_ack(pkt->identifier, pkt->data, pkt->data_len);
}
/*
True when both sides’ IPCP Configure-Requests are acked.
*/
int ipcp_is_open(void) {
    return g_ipcp_state == IPCP_STATE_OPENED;
}

uint32_t ipcp_get_our_ip_address(void) {
    return g_ipcp_cfg.our_ip_address;
}

uint32_t ipcp_get_peer_ip_address(void) {
    return g_peer_ip_address;
}

/*
Main IPCP dispatcher:
    parse + print packet
    route by code (CONFIG_REQ/ACK/NAK/REJ/TERM_REQ/TERM_ACK)
    update state and send responses as needed.
*/
int ipcp_handle_packet(const uint8_t *info, unsigned info_len) {
    ipcp_pkt_t pkt;
    int ret = ipcp_parse(info, info_len, &pkt);
    int r;

    if (ret < 0) {
        printk("IPCP: parse failed %d\n", ret);
        return ret;
    }

    ipcp_print_packet(&pkt);

    switch (pkt.code) {
    case IPCP_CONFIG_REQ:
        return ipcp_handle_config_req(&pkt);

    case IPCP_CONFIG_ACK:
        printk("IPCP: received Configure-Ack\n");
        if (pkt.identifier == g_our_last_confreq_id) {
            g_our_confreq_acked = 1;
            printk("IPCP: our Configure-Request was acked\n");
            ipcp_update_state();
        }
        return IPCP_OK;

    case IPCP_CONFIG_NAK:
        printk("IPCP: received Configure-Nak\n");
        return ipcp_handle_config_nak(&pkt);

    case IPCP_CONFIG_REJ:
        printk("IPCP: received Configure-Reject\n");
        return ipcp_handle_config_rej(&pkt);

    case IPCP_TERM_REQ:
        r = ipcp_send_term_ack(pkt.identifier, pkt.data, pkt.data_len);
        ipcp_reset_state();
        return r;

    case IPCP_TERM_ACK:
        printk("IPCP: received Terminate-Ack\n");
        ipcp_reset_state();
        return IPCP_OK;

    default:
        printk("IPCP: unsupported code=%d\n", pkt.code);
        return IPCP_OK;
    }
}
