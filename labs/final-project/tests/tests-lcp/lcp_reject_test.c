#include "rpi.h"
#include "lcp.h"
/*
Expected Output:
LCP: code=1 id=5 len=8 data_len=4
LCP: rejecting unsupported option type=99 len=4
LCP: sending Configure-Reject
*/
void notmain(void) {
    lcp_config_t cfg = {
        .wanted_mru = 1500,
        .wanted_accm = 0xFFFFFFFF,
        .magic_number = 0x12345678,
    };
    lcp_init(&cfg);

    // Unsupported option type 99, len 4, data 00 01
    uint8_t pkt[] = {
        0x01, 0x05, 0x00, 0x08,
        0x63, 0x04, 0x00, 0x01
    };

    int r = lcp_handle_packet(pkt, sizeof(pkt));
    printk("lcp_handle_packet returned %d\n", r);

    clean_reboot();
}