#include "rpi.h"
#include "ppp.h"
#include "lcp.h"
#include "ipcp.h"
#include "ip.h"

// swallow printk bytes once uart is carrying ppp traffic
static int drop_putc(int chr) {
    (void)chr;
    return 1;
}

// bring up ppp, negotiate ip, and then leave ip_handle_packet running as a tiny http server
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
    // keep printk off the ppp uart so we do not corrupt the wire protocol
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
        .our_ip_address = 0x0A1B0C1D, // 10.27.12.29
    };
    ipcp_init(&ipcp_cfg);

    while (1) {
        // block waiting for the next decoded ppp packet from the laptop
        int r = ppp_recv(&pkt, buf, sizeof buf);
        if (r < 0)
            continue;

        if (pkt.protocol == PPP_PROTO_LCP) {
            lcp_pkts++;
            // wait for the peer to start talking, then send our first lcp request
            if (!sent_lcp) {
                lcp_send_config_req(lcp_id++);
                sent_lcp = 1;
            }
        } else if (pkt.protocol == PPP_PROTO_IPCP) {
            ipcp_pkts++;
        }

        // once ppp strips framing, dispatch the payload to lcp, ipcp, or ip
        ppp_dispatch(&pkt, lcp_handle_packet, ipcp_handle_packet, ip_handle_packet);

        // after lcp opens the link, ask for our ip address with ipcp
        if (lcp_is_open() && !sent_ipcp) {
            ipcp_send_config_req(ipcp_id++);
            sent_ipcp = 1;
        }

        // retransmit if negotiation seems stuck
        if (pkt.protocol == PPP_PROTO_LCP && !lcp_is_open() && lcp_pkts % 8 == 0)
            lcp_send_config_req(lcp_id++);
        if (pkt.protocol == PPP_PROTO_IPCP && lcp_is_open() && !ipcp_is_open() && ipcp_pkts % 8 == 0)
            ipcp_send_config_req(ipcp_id++);
    }
}
