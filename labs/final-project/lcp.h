#ifndef LCP_H
#define LCP_H

#include "rpi.h"
#include "ppp.h"

/*
 * LCP runs inside PPP with protocol = 0xC021.
 *
 * LCP packet format:
 *   Code       (1 byte)
 *   Identifier (1 byte)
 *   Length     (2 bytes, big-endian)
 *   Data       (Length - 4 bytes)
 */
 

/* LCP packet codes */
enum {
    LCP_CONFIG_REQ  = 1,
    LCP_CONFIG_ACK  = 2,
    LCP_CONFIG_NAK  = 3,
    LCP_CONFIG_REJ  = 4,
    LCP_TERM_REQ    = 5,
    LCP_TERM_ACK    = 6,
    LCP_ECHO_REQ    = 9,
    LCP_ECHO_REPLY  = 10,
};

/* LCP states */
typedef enum {
    LCP_STATE_CLOSED = 0,
    LCP_STATE_REQ_SENT,
    LCP_STATE_ACK_RCVD,   // peer ACKed our Configure-Request
    LCP_STATE_ACK_SENT,   // we ACKed peer's Configure-Request
    LCP_STATE_OPENED
} lcp_state_t;

/* LCP option types */
enum {
    LCP_OPT_MRU          = 1,
    LCP_OPT_ACCM         = 2,
    LCP_OPT_MAGIC_NUMBER = 5,
};

/* Return codes */
enum {
    LCP_OK        = 0,
    LCP_ERR       = -1,
    LCP_BAD_PKT   = -2,
    LCP_BAD_LEN   = -3,
    LCP_TOO_LARGE = -4,
};

/* Minimal LCP configuration we support */
typedef struct {
    uint16_t wanted_mru;       /* usually 1500 */
    int want_mru;
    uint32_t wanted_accm;  
    int want_accm;    /* usually 0xFFFFFFFF or 0 */
    uint32_t magic_number;     /* our magic number */
    int want_magic_number;
} lcp_config_t;

/* Parsed LCP packet */
typedef struct {
    uint8_t code; // what kind of packet is it? ex) LCP_CONFIG_REQ, LCP_CONFIG_ACK, LCP_CONFIG_NAK, LCP_CONFIG_REJ, LCP_TERM_REQ, LCP_TERM_ACK, LCP_ECHO_REQ, LCP_ECHO_REPLY
    uint8_t identifier; //lets both sides track which request the reply belongs to. ex) Reply to Request #7
    uint16_t length;           /* full LCP packet length, including 4-byte header (code, identifier, length) */
    const uint8_t *data;       /* points into caller buffer */
    unsigned data_len;         /* length - 4 */
} lcp_pkt_t;

/* Global config init */
void lcp_init(const lcp_config_t *cfg);

/* Parse and print */
int lcp_parse(const uint8_t *info, unsigned info_len, lcp_pkt_t *pkt);
void lcp_print_packet(const lcp_pkt_t *pkt);

/* Main handler: suitable for ppp_dispatch(..., lcp_handle_packet, ...) */
int lcp_handle_packet(const uint8_t *info, unsigned info_len);

int lcp_send_config_req(uint8_t identifier);

int lcp_is_open(void);

#endif