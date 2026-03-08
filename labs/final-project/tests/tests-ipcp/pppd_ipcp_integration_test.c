#include "rpi.h"
#include "ppp.h"
#include "lcp.h"
#include "ipcp.h"

static int drop_putc(int chr) {
    (void)chr;
    return 1;
}

static int ip_stub(const uint8_t *info, unsigned info_len) {
    (void)info;
    (void)info_len;
    return 0;
}

void notmain(void) {
    uint8_t buf[1600];
    ppp_pkt_t pkt;
    uint8_t lcp_id = 1;
    uint8_t ipcp_id = 1;
    unsigned lcp_pkts = 0;
    unsigned ipcp_pkts = 0;
    int sent_lcp = 0;
    int sent_ipcp = 0;

    uart_init();
    // Avoid debug text corrupting PPP stream.
    rpi_putchar_set(drop_putc);

    lcp_config_t lcp_cfg = {
        .want_mru = 1,
        .want_accm = 1,
        .want_magic_number = 1,
        .wanted_mru = 1500,
        .wanted_accm = 0xFFFFFFFF,
        .magic_number = 0x12345678,
    };
    lcp_init(&lcp_cfg);

    ipcp_config_t ipcp_cfg = {
        .want_ip_address = 1,
        // don't set address as 0.0.0.0 because it will be rejected by pppd. then, we won't get a suggested address. 
        .our_ip_address = 0x0A1B0C1B, // we are going to send a request if we can use this ip address 
    };
    ipcp_init(&ipcp_cfg);

    while (1) {
        int r = ppp_recv(&pkt, buf, sizeof buf);
        if (r < 0)
            continue;

        if (pkt.protocol == PPP_PROTO_LCP) {
            lcp_pkts++;
            if (!sent_lcp) {
                lcp_send_config_req(lcp_id++);
                sent_lcp = 1;
            }
        } else if (pkt.protocol == PPP_PROTO_IPCP) {
            ipcp_pkts++;
        }

        ppp_dispatch(&pkt, lcp_handle_packet, ipcp_handle_packet, ip_stub);

        if (lcp_is_open() && !sent_ipcp) {
            ipcp_send_config_req(ipcp_id++);
            sent_ipcp = 1;
        }

        // Retransmit conservatively if stuck in negotiation.
        if (pkt.protocol == PPP_PROTO_LCP && !lcp_is_open() && lcp_pkts % 8 == 0) {
            lcp_send_config_req(lcp_id++);
        }
        if (pkt.protocol == PPP_PROTO_IPCP && lcp_is_open() && !ipcp_is_open() && ipcp_pkts % 8 == 0) {
            ipcp_send_config_req(ipcp_id++);
        }
    }
}
