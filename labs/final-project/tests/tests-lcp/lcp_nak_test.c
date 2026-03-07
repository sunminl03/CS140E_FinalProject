#include "rpi.h"
#include "lcp.h"
/*
Expected: 
LCP: code=1 id=3 len=8 data_len=4
LCP: peer MRU=296
LCP: sending Configure-Nak
*/
void notmain(void) {
    lcp_config_t cfg = {
        .wanted_mru = 1500,
        .wanted_accm = 0xFFFFFFFF,
        .magic_number = 0x12345678,
    };
    lcp_init(&cfg);

    // Peer asks for MRU = 296 (0x0128)
    uint8_t pkt[] = {
        0x01, 0x03, 0x00, 0x08,
        0x01, 0x04, 0x01, 0x28
    };

    int r = lcp_handle_packet(pkt, sizeof(pkt));
    printk("lcp_handle_packet returned %d\n", r);

    clean_reboot();
}