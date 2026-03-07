#include "rpi.h"
#include "lcp.h"
/*Expected Output:
lcp_parse returned 0
LCP: code=1 id=7 len=14 data_len=10
*/
void notmain(void) {
    // LCP Configure-Request:
    // code=1, id=7, len=14
    // options:
    //   MRU   = 1500    -> 01 04 05 DC
    //   Magic = 0x12345678 -> 05 06 12 34 56 78
    uint8_t pkt_bytes[] = {
        0x01, 0x07, 0x00, 0x0E,
        0x01, 0x04, 0x05, 0xDC,
        0x05, 0x06, 0x12, 0x34, 0x56, 0x78
    };

    lcp_pkt_t pkt;
    int r = lcp_parse(pkt_bytes, sizeof(pkt_bytes), &pkt);

    printk("lcp_parse returned %d\n", r);
    if (r == 0) {
        lcp_print_packet(&pkt);
    }

    clean_reboot();
}