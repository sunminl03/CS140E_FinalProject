#include "rpi.h"
#include "ppp.h"
#include "lcp.h"

static int ipcp_stub(const uint8_t *info, unsigned info_len) {
    (void)info;
    (void)info_len;
    return 0;
}

static int ip_stub(const uint8_t *info, unsigned info_len) {
    (void)info;
    (void)info_len;
    return 0;
}

static int drop_putc(int chr) {
    (void)chr;
    return 1;
}

void notmain(void) {
    uint8_t buf[1600];
    ppp_pkt_t pkt;
    int our_confreq_sent = 0;

    uart_init();
    // Disable printk output on the PPP UART to avoid corrupting HDLC frames.
    rpi_putchar_set(drop_putc);

    lcp_config_t cfg = {
        .wanted_mru = 1500,
        .wanted_accm = 0xFFFFFFFF,
        .magic_number = 0x12345678,
    };
    lcp_init(&cfg);

    while (1) {
        int r = ppp_recv(&pkt, buf, sizeof buf);
        if (r < 0)
            continue;

        // Start our side once link traffic is observed, then let LCP core handle all replies.
        if (!our_confreq_sent && pkt.protocol == PPP_PROTO_LCP) {
            lcp_send_config_req(1);
            our_confreq_sent = 1;
        }

        ppp_dispatch(&pkt, lcp_handle_packet, ipcp_stub, ip_stub);
    }
}
