#include "rpi.h"
#include "ppp.h"
#include "lcp.h"
#include "ipcp.h"
#include "ip.h"

static int drop_putc(int chr) {
    (void)chr;
    return 1;
}

void notmain(void) {
    uint8_t buf[1600];
    ppp_pkt_t pkt;
    int sent_lcp_req = 0;
    int sent_ipcp_req = 0;

    uart_init();
    // Keep printk off the PPP UART.
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
        int r = ppp_recv(&pkt, buf, sizeof buf);
        if (r < 0)
            continue;

        if (!sent_lcp_req && pkt.protocol == PPP_PROTO_LCP) {
            lcp_send_config_req(1);
            sent_lcp_req = 1;
        }

        if (lcp_is_open() && !sent_ipcp_req) {
            ipcp_send_config_req(1);
            sent_ipcp_req = 1;
        }

        // Once IP packets arrive (e.g., ping), ip_handle_packet should
        // generate echo replies via ip_send.
        ppp_dispatch(&pkt, lcp_handle_packet, ipcp_handle_packet, ip_handle_packet);
    }
}
