#include "rpi.h"
#include "lcp.h"

/*
Expected Output: Negative Value 
*/
void notmain(void) {
    // Says length = 20, but actual buffer is only 8 bytes
    uint8_t bad_pkt[] = {
        0x01, 0x01, 0x00, 0x14,
        0x01, 0x04, 0x05, 0xDC
    };

    lcp_pkt_t pkt;
    int r = lcp_parse(bad_pkt, sizeof(bad_pkt), &pkt);

    printk("bad parse returned %d (expect negative)\n", r);
    clean_reboot();
}