#include "rpi.h"
#include "ppp.h"
#include "lcp.h"
/*
Expected Output:
LCP full-path test: waiting for one PPP packet...
PPP receive succeeded
PPP: addr=0x0xff ctrl=0x0x3 proto=0x0xc021 info_len=20
LCP: peer MRU=1500
LCP: peer ACCM=0xffffffff
LCP: peer magic=0xcafebabe
LCP: sending Configure-Ack
n~ppp_dispatch returned Configure-Ack
*/
static int ipcp_stub(const uint8_t *info, unsigned info_len) {
    // printk("IPCP stub called: len=%d\n", info_len);
    return 0;
}

static int ip_stub(const uint8_t *info, unsigned info_len) {
    // printk("IP stub called: len=%d\n", info_len);
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

    printk("LCP full-path test: waiting for one PPP packet...\n");

    int r = ppp_recv(&pkt, buf, sizeof buf);
    if (r < 0) {
        printk("ppp_recv failed: %d\n", r);
        clean_reboot();
    }

    printk("PPP receive succeeded\n");
    ppp_print_packet(&pkt);

    r = ppp_dispatch(&pkt, lcp_handle_packet, ipcp_stub, ip_stub);
    if (r == 0) {
        printk("ppp_dispatch returned Configure-Ack\n");
    } else {
        printk("ppp_dispatch returned %d\n", r);
    }
    

    clean_reboot();

}