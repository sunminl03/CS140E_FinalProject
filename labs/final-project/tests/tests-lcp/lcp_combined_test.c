#include "rpi.h"
#include "lcp.h"
/*
Expected Output:
=== ACK case ===
LCP: code=1 id=9 len=20 data_len=16
LCP: peer MRU=1500
LCP: peer ACCM=ffffffff
LCP: peer magic=cafebabe
LCP: sending Configure-Ack
<binary PPP/HDLC bytes may print here>
return=0

=== NAK case ===
LCP: code=1 id=3 len=8 data_len=4
LCP: peer MRU=296
LCP: sending Configure-Nak
<binary PPP/HDLC bytes may print here>
return=0

=== REJECT case ===
LCP: code=1 id=5 len=8 data_len=4
LCP: rejecting unsupported option type=99 len=4
LCP: sending Configure-Reject
<binary PPP/HDLC bytes may print here>
return=0

=== ECHO case ===
LCP: code=9 id=4 len=8 data_len=4
LCP: received Echo-Request, sending Echo-Reply
<binary PPP/HDLC bytes may print here>
return=0

*/
static void run_test(const char *name, uint8_t *pkt, unsigned len) {
    printk("\n=== %s ===\n", name);
    int r = lcp_handle_packet(pkt, len);
    printk("return=%d\n", r);
}

void notmain(void) {
    lcp_config_t cfg = {
        .wanted_mru = 1500,
        .wanted_accm = 0xFFFFFFFF,
        .magic_number = 0x12345678,
    };
    lcp_init(&cfg);

    uint8_t ack_pkt[] = {
        0x01, 0x09, 0x00, 0x14,
        0x01, 0x04, 0x05, 0xDC,
        0x02, 0x06, 0xFF, 0xFF, 0xFF, 0xFF,
        0x05, 0x06, 0xCA, 0xFE, 0xBA, 0xBE
    };

    uint8_t nak_pkt[] = {
        0x01, 0x03, 0x00, 0x08,
        0x01, 0x04, 0x01, 0x28
    };

    uint8_t rej_pkt[] = {
        0x01, 0x05, 0x00, 0x08,
        0x63, 0x04, 0x00, 0x01
    };

    uint8_t echo_pkt[] = {
        0x09, 0x04, 0x00, 0x08,
        0x12, 0x34, 0x56, 0x78
    };

    run_test("ACK case", ack_pkt, sizeof(ack_pkt));
    run_test("NAK case", nak_pkt, sizeof(nak_pkt));
    run_test("REJECT case", rej_pkt, sizeof(rej_pkt));
    run_test("ECHO case", echo_pkt, sizeof(echo_pkt));

    clean_reboot();
}