#include "rpi.h"
#include "ppp.h"

/*
Expected output: PPP: addr=ff ctrl=03 proto=c021 info_len=3
When pi receives the frame, we decode it to get payload then look at the protocol bytes to decide which handler to call
*/
void notmain(void) {
    uart_init();

    uint8_t buf[1600];
    ppp_pkt_t pkt;

    printk("waiting for PPP frame...\n");
    // check if we can receive from laptop and can pass to right handler by reading protocol bytes
    int r = ppp_recv(&pkt, buf, sizeof buf);
    if (r < 0) {
        printk("ppp_recv failed: %d\n", r);
        clean_reboot();
    }

    ppp_print_packet(&pkt);
    clean_reboot();
}