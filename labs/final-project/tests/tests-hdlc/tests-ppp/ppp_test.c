#include "rpi.h"
#include "ppp.h"
/*
Expected output:
PPP TEST START
ppp_build -> 9 bytes
ppp_parse -> 0
PPP: addr=0xff ctrl=0x3 proto=0xc021 info_len=5
LCP handler called
PPP TEST DONE
*/

static int lcp_handler(const uint8_t *info, unsigned info_len) {
    printk("LCP handler called, len=%d\n", info_len);
    return 0;
}

static int ipcp_handler(const uint8_t *info, unsigned info_len) {
    printk("IPCP handler called\n");
    return 0;
}

static int ip_handler(const uint8_t *info, unsigned info_len) {
    printk("IP handler called\n");
    return 0;
}

void notmain(void) {

    uint8_t pkt[1600];
    uint8_t buf[1600];

    ppp_pkt_t parsed;

    printk("PPP TEST START\n");

    // fake payload
    uint8_t payload[] = {1,2,3,4,5};

    // build PPP packet
    int n = ppp_build(
        PPP_PROTO_LCP,
        payload,
        sizeof(payload),
        pkt,
        sizeof(pkt)
    );

    printk("ppp_build -> %d bytes\n", n);

    // parse it back
    int r = ppp_parse(pkt, n, &parsed);

    printk("ppp_parse -> %d\n", r);

    ppp_print_packet(&parsed);

    // dispatch
    ppp_dispatch(
        &parsed,
        lcp_handler,
        ipcp_handler,
        ip_handler
    );

    printk("PPP TEST DONE\n");

    clean_reboot();
}