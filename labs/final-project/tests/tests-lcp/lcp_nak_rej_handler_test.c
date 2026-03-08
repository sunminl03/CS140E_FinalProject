#include "rpi.h"
#include "lcp.h"

/*
 * Drives the new Configure-Nak / Configure-Reject handlers.
 *
 * Expected high-level behavior:
 * 1) We send ConfReq id=7.
 * 2) Peer sends ConfNak id=7 with suggested ACCM=0x0.
 *    -> handler should accept suggestion and send new ConfReq id=8.
 * 3) Peer sends ConfRej id=8 rejecting ACCM.
 *    -> handler should disable ACCM and send new ConfReq id=9.
 * 4) Peer sends stale ConfNak id=7.
 *    -> ignored as old id, no resend.
 */
void notmain(void) {
    lcp_config_t cfg = {
        .want_mru = 1,
        .want_accm = 1,
        .want_magic_number = 1,
        .wanted_mru = 1500,
        .wanted_accm = 0xFFFFFFFF,
        .magic_number = 0x12345678,
    };
    lcp_init(&cfg);

    // Establish request-in-flight state.
    lcp_send_config_req(7);

    // Configure-Nak(id=7): suggest ACCM = 0x00000000.
    uint8_t conf_nak[] = {
        0x03, 0x07, 0x00, 0x0A,
        0x02, 0x06, 0x00, 0x00, 0x00, 0x00
    };
    int r1 = lcp_handle_packet(conf_nak, sizeof conf_nak);
    printk("NAK handler returned %d\n", r1);

    // Configure-Reject(id=8): reject ACCM option.
    uint8_t conf_rej[] = {
        0x04, 0x08, 0x00, 0x0A,
        0x02, 0x06, 0x00, 0x00, 0x00, 0x00
    };
    int r2 = lcp_handle_packet(conf_rej, sizeof conf_rej);
    printk("REJ handler returned %d\n", r2);

    // Old-id Configure-Nak should be ignored.
    uint8_t stale_nak[] = {
        0x03, 0x07, 0x00, 0x0A,
        0x02, 0x06, 0xFF, 0xFF, 0xFF, 0xFF
    };
    int r3 = lcp_handle_packet(stale_nak, sizeof stale_nak);
    printk("stale NAK returned %d (expect %d)\n", r3, LCP_OK);

    clean_reboot();
}
