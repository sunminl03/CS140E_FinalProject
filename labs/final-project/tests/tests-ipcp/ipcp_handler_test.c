#include "rpi.h"
#include "lcp.h"
#include "ipcp.h"

/*
 * Deterministic IPCP handler test (no laptop needed).
 *
 * Flow:
 *  1) Force LCP OPENED so ipcp_send_config_req() is allowed.
 *  2) Send our IPCP ConfReq id=5 (IP=0.0.0.0).
 *  3) Feed IPCP ConfNak id=5 with suggested IP 10.0.0.2.
 *     -> handler should resend IPCP ConfReq id=6 with that IP.
 *  4) Feed IPCP ConfAck id=6.
 *  5) Feed peer IPCP ConfReq (IP=10.0.0.1), we should ACK it.
 *  6) Verify ipcp_is_open() == 1 and learned addresses.
 */
void notmain(void) {
    lcp_config_t lcp_cfg = {
        .want_mru = 0,
        .want_accm = 0,
        .want_magic_number = 1,
        .wanted_mru = 1500,
        .wanted_accm = 0xFFFFFFFF,
        .magic_number = 0x12345678,
    };
    lcp_init(&lcp_cfg);

    // Open LCP in a scripted way.
    lcp_send_config_req(1);
    uint8_t lcp_ack_our_req[] = {
        0x02, 0x01, 0x00, 0x0A, 0x05, 0x06, 0x12, 0x34, 0x56, 0x78
    };
    lcp_handle_packet(lcp_ack_our_req, sizeof lcp_ack_our_req);
    uint8_t lcp_peer_req[] = {
        0x01, 0x11, 0x00, 0x0A, 0x05, 0x06, 0xAA, 0xBB, 0xCC, 0xDD
    };
    lcp_handle_packet(lcp_peer_req, sizeof lcp_peer_req);

    ipcp_config_t ipcp_cfg = {
        .want_ip_address = 1,
        .our_ip_address = 0x00000000,    // 0.0.0.0
    };
    ipcp_init(&ipcp_cfg);

    int s = ipcp_send_config_req(5);
    printk("send ConfReq id=5 ret=%d\n", s);

    uint8_t ipcp_nak[] = {
        0x03, 0x05, 0x00, 0x0A, 0x03, 0x06, 0x0A, 0x00, 0x00, 0x02
    };
    int r1 = ipcp_handle_packet(ipcp_nak, sizeof ipcp_nak);
    printk("after NAK: ret=%d our_ip=0x%x open=%d\n",
           r1, ipcp_get_our_ip_address(), ipcp_is_open());

    uint8_t ipcp_ack[] = {
        0x02, 0x06, 0x00, 0x0A, 0x03, 0x06, 0x0A, 0x00, 0x00, 0x02
    };
    int r2 = ipcp_handle_packet(ipcp_ack, sizeof ipcp_ack);
    printk("after ACK: ret=%d our_ip=0x%x open=%d\n",
           r2, ipcp_get_our_ip_address(), ipcp_is_open());

    uint8_t ipcp_peer_req[] = {
        0x01, 0x21, 0x00, 0x0A, 0x03, 0x06, 0x0A, 0x00, 0x00, 0x01
    };
    int r3 = ipcp_handle_packet(ipcp_peer_req, sizeof ipcp_peer_req);
    printk("after peer ConfReq: ret=%d peer_ip=0x%x open=%d (expect 1)\n",
           r3, ipcp_get_peer_ip_address(), ipcp_is_open());

    clean_reboot();
}
