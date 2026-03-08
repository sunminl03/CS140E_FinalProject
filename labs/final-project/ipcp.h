#ifndef __IPCP_H__
#define __IPCP_H__

#include "rpi.h"
#include "ppp.h"

enum {
    IPCP_OK        = 0,
    IPCP_ERR       = -1,
    IPCP_BAD_LEN   = -2,
    IPCP_BAD_PKT   = -3,
    IPCP_TOO_LARGE = -4,
    IPCP_NOT_READY = -5,
};

/* IPCP packet codes */
enum {
    IPCP_CONFIG_REQ = 1,
    IPCP_CONFIG_ACK = 2,
    IPCP_CONFIG_NAK = 3,
    IPCP_CONFIG_REJ = 4,
    IPCP_TERM_REQ   = 5,
    IPCP_TERM_ACK   = 6,
};

/* IPCP options */
enum {
    IPCP_OPT_IP_COMPRESSION_PROTOCOL = 2,
    IPCP_OPT_IP_ADDRESS              = 3,
};

typedef enum {
    IPCP_STATE_CLOSED = 0,
    IPCP_STATE_REQ_SENT,
    IPCP_STATE_ACK_RCVD,
    IPCP_STATE_ACK_SENT,
    IPCP_STATE_OPENED,
} ipcp_state_t;

typedef struct {
    uint8_t  code;
    uint8_t  identifier;
    uint16_t length;
    const uint8_t *data;
    unsigned data_len;
} ipcp_pkt_t;

typedef struct {
    int want_ip_address;       /* include IP-Address option in our ConfReq */
    uint32_t our_ip_address;   /* our desired IP, initially 0.0.0.0 */
} ipcp_config_t;

void ipcp_init(const ipcp_config_t *cfg);

int ipcp_parse(const uint8_t *info, unsigned info_len, ipcp_pkt_t *pkt);
void ipcp_print_packet(const ipcp_pkt_t *pkt);

int ipcp_send_config_req(uint8_t identifier);
int ipcp_handle_packet(const uint8_t *info, unsigned info_len);

int ipcp_is_open(void);

uint32_t ipcp_get_our_ip_address(void);
uint32_t ipcp_get_peer_ip_address(void);

#endif