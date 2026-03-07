#include "rpi.h"
#include "ppp.h"
#include "lcp.h"

static int ipcp_stub(const uint8_t *info, unsigned info_len) {
    return 0;
}

static int ip_stub(const uint8_t *info, unsigned info_len) {
    return 0;
}

void notmain(void) {
    uint8_t buf[1600];
    ppp_pkt_t pkt;

    uart_init();

    lcp_config_t cfg = {
        .wanted_mru = 1500,
        .wanted_accm = 0xFFFFFFFF,
        .magic_number = 0x12345678,
    };
    lcp_init(&cfg);

    // Keep startup text minimal.
    printk("PPP Pi ready\n");
    // printk("about to send our LCP ConfReq\n");
    // lcp_send_config_req(3);
    // printk("finished lcp_send_config_req\n");

    while (1) {
        int r = ppp_recv(&pkt, buf, sizeof buf);
        if (r < 0) {
            continue;
        }

        ppp_dispatch(&pkt, lcp_handle_packet, ipcp_stub, ip_stub);
    }
}