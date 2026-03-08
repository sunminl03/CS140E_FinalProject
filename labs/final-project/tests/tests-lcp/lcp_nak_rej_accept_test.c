#include "rpi.h"
#include "lcp.h"

/*
 * Demonstrates end-to-end convergence:
 *   1) We send ConfReq id=7 (MRU + ACCM + Magic).
 *   2) Peer NAKs id=7 with ACCM suggestion.
 *   3) Peer REJs id=8 for ACCM.
 *   4) Peer ACKs id=9 (MRU + Magic only).
 *   5) Peer sends ConfReq; we ACK it.
 *   6) lcp_is_open() should become 1.
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

    // Start with our request id=7.
    lcp_send_config_req(7);

    // Peer NAKs id=7 and suggests ACCM=0x00000000.
    uint8_t conf_nak[] = {
        0x03, 0x07, 0x00, 0x0A,
        0x02, 0x06, 0x00, 0x00, 0x00, 0x00
    };
    int r1 = lcp_handle_packet(conf_nak, sizeof conf_nak);
    printk("after NAK: ret=%d open=%d\n", r1, lcp_is_open());

    // Peer REJs id=8 for ACCM; we should drop ACCM and resend as id=9.
    uint8_t conf_rej[] = {
        0x04, 0x08, 0x00, 0x0A,
        0x02, 0x06, 0x00, 0x00, 0x00, 0x00
    };
    int r2 = lcp_handle_packet(conf_rej, sizeof conf_rej);
    printk("after REJ: ret=%d open=%d\n", r2, lcp_is_open());

    // ACK our latest id=9. Data must match our id=9 ConfReq (MRU + Magic).
    uint8_t conf_ack_9[] = {
        0x02, 0x09, 0x00, 0x0E,
        0x01, 0x04, 0x05, 0xDC,
        0x05, 0x06, 0x12, 0x34, 0x56, 0x78
    };
    int r3 = lcp_handle_packet(conf_ack_9, sizeof conf_ack_9);
    printk("after ACK(id=9): ret=%d open=%d\n", r3, lcp_is_open());

    // Peer request (Magic only): we should ACK it and enter OPENED.
    uint8_t peer_conf_req[] = {
        0x01, 0x21, 0x00, 0x0A,
        0x05, 0x06, 0xAA, 0xBB, 0xCC, 0xDD
    };
    int r4 = lcp_handle_packet(peer_conf_req, sizeof peer_conf_req);
    printk("after peer ConfReq: ret=%d open=%d (expect 1)\n", r4, lcp_is_open());

    clean_reboot();
}
