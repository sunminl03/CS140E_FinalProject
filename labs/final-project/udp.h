#ifndef __UDP_H__
#define __UDP_H__

#include "rpi.h"

// udp header length: exactly 8 bytes
#define UDP_HDR_LEN 8
#define UDP_ECHO_PORT 9000 // set port as 9000

// udp header
typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t len;
    uint16_t checksum;
    const uint8_t *payload;
    unsigned payload_len;
} udp_hdr_t;

// return codes
// return codes
enum {
    UDP_OK        = 0,
    UDP_ERR       = -1,
    UDP_BAD_HDR   = -2,
    UDP_BAD_CKSUM = -3,
    UDP_WRONG_SIZE = -4,
};

// parse a raw udp packet into its udp header
int udp_parse(const uint8_t *pkt, unsigned pkt_len, udp_hdr_t *hdr);

// compute checksum on udp datagram
uint16_t udp_checksum(uint32_t src_ip, uint32_t dst_ip, const uint8_t *udp_pkt, unsigned udp_len);

// udp receive handler
int udp_handle_packet(uint32_t src_ip, uint32_t dst_ip, const uint8_t *pkt, unsigned pkt_len);

#endif
