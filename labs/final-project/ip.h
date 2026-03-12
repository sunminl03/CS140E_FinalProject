#ifndef __IP_H__
#define __IP_H__

#include "rpi.h"
#include "ppp.h"

// ip header length (no options)
#define IP_HDR_LEN 20

// protocol numbers
enum {
    IP_PROTO_ICMP = 1,
    IP_PROTO_TCP  = 6,
    IP_PROTO_UDP  = 17,
};

// added for ICMP echo request/reply
struct icmp_echo {
    uint8_t  type;      // 8 req, 0 reply
    uint8_t  code;      // 0
    uint16_t checksum;
    uint16_t ident;
    uint16_t seq;
    uint8_t  data[];    // variable
};

// return codes
enum {
    IP_OK        = 0,
    IP_ERR       = -1,
    IP_BAD_HDR   = -2,
    IP_BAD_CKSUM = -3,
    IP_TOO_SHORT = -4,
};

// ipv4 header
typedef struct {
    uint8_t  version;
    uint8_t  ihl; // header length in 32-bit words
    uint8_t  tos;
    uint16_t total_len;
    uint16_t id;
    uint8_t  flags; // 3 bits: reserved, DF, MF
    uint16_t frag_offset;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;
    uint32_t src;
    uint32_t dst;
    const uint8_t *payload;
    unsigned payload_len;
} ip_hdr_t;

// ones' complement checksum over len bytes
uint16_t ip_checksum(const uint8_t *data, unsigned len);

// parse a raw ip packet into its ipv4 header. validates version, ihl, checksum.
int ip_parse(const uint8_t *pkt, unsigned pkt_len, ip_hdr_t *hdr);

// build a raw ip packet into out[]. returns total length or negative on error.
int ip_build(const ip_hdr_t *hdr, const uint8_t *payload, unsigned payload_len,
             uint8_t *out, unsigned out_size);

// send an ip packet over ppp
int ip_send(uint32_t src, uint32_t dst, uint8_t protocol,
            const uint8_t *payload, unsigned payload_len);

// print ip header for debugging
void ip_print_hdr(const ip_hdr_t *hdr);

// handler suitable for ppp_dispatch ip_handler
int ip_handle_packet(const uint8_t *info, unsigned info_len);

#endif
