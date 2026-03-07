#ifndef PPP_H
#define PPP_H

#include "rpi.h"
#include "hdlc_framing.h"

/*
 * Minimal PPP layer for PPP over HDLC-like framing.
 *
 * PPP frame payload format (before HDLC framing):
 *   Address  (1 byte) = 0xFF
 *   Control  (1 byte) = 0x03
 *   Protocol (2 bytes, big-endian)
 *   Information field (0 or more bytes)
 *
 * Then this whole PPP payload is passed to hdlc_send().
 */

/* Standard PPP address/control values */
enum {
    PPP_ADDRESS = 0xFF,
    PPP_CONTROL = 0x03,
};

/* Common PPP protocol numbers */
enum {
    PPP_PROTO_IP   = 0x0021,
    PPP_PROTO_LCP  = 0xC021,
    PPP_PROTO_IPCP = 0x8021,
};

/* Generic PPP return codes */
enum {
    PPP_OK          = 0,
    PPP_ERROR       = -1,
    PPP_TOO_LARGE   = -2,
    PPP_BAD_FRAME   = -3,
    PPP_UNSUPPORTED = -4,
};

/* Max PPP info field we will parse/store in one packet */
#ifndef PPP_MAX_INFO_LEN
#define PPP_MAX_INFO_LEN 1500
#endif

/* Parsed PPP packet */
typedef struct {
    uint8_t  address;      /* should be 0xFF */
    uint8_t  control;      /* should be 0x03 */
    uint16_t protocol;     /* e.g. 0xC021, 0x8021, 0x0021 */
    const uint8_t *info;   /* points into caller's input buffer */
    unsigned info_len;
} ppp_pkt_t;

/*
 * Protocol handler function type.
 * info/info_len point to the PPP information field only
 * (the address/control/protocol have already been stripped).
 */
typedef int (*ppp_protocol_handler_t)(const uint8_t *info, unsigned info_len);

/* Core parse/build/send/recv helpers */
int ppp_parse(const uint8_t *buf, unsigned len, ppp_pkt_t *pkt);
int ppp_build(uint16_t protocol,
              const uint8_t *info, unsigned info_len,
              uint8_t *out, unsigned out_size);
int ppp_send(uint16_t protocol, const uint8_t *info, unsigned info_len);
int ppp_recv(ppp_pkt_t *pkt, uint8_t *buf, unsigned buf_size);

/* Dispatch helper */
int ppp_dispatch(const ppp_pkt_t *pkt,
                 ppp_protocol_handler_t lcp_handler,
                 ppp_protocol_handler_t ipcp_handler,
                 ppp_protocol_handler_t ip_handler);

/* Debug helper */
void ppp_print_packet(const ppp_pkt_t *pkt);

#endif