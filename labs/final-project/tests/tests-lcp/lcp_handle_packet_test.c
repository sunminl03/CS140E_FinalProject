#include "rpi.h"
#include "lcp.h"

/*
Expected Output:
LCP: code=1 id=9 len=20 data_len=16
LCP: peer MRU=1500
LCP: peer ACCM=ffffffff
LCP: peer magic=cafebabe
LCP: sending Configure-Ack
*/
void notmain(void) {
    lcp_config_t cfg = {
        .wanted_mru = 1500,
        .wanted_accm = 0xFFFFFFFF,
        .magic_number = 0x12345678,
    };
    lcp_init(&cfg);

    // Configure-Request:
    // MRU=1500
    // ACCM=FFFFFFFF
    // Magic=CAFEBABE
    uint8_t pkt[] = {
        0x01, 0x09, 0x00, 0x14,
        0x01, 0x04, 0x05, 0xDC,
        0x02, 0x06, 0xFF, 0xFF, 0xFF, 0xFF,
        0x05, 0x06, 0xCA, 0xFE, 0xBA, 0xBE
    };

    int r = lcp_handle_packet(pkt, sizeof(pkt));
    printk("lcp_handle_packet returned %d\n", r);

    clean_reboot();
}