#ifndef __TCP_H__
#define __TCP_H__

#include "rpi.h"

#define TCP_HDR_LEN 20
#define TCP_MAX_HDR_LEN 60

enum {
    TCP_FIN = 0x01,
    TCP_SYN = 0x02,
    TCP_RST = 0x04,
    TCP_PSH = 0x08,
    TCP_ACK = 0x10,
    TCP_URG = 0x20,
    TCP_ECE = 0x40,
    TCP_CWR = 0x80,
};

enum {
    TCP_OK        = 0,
    TCP_ERR       = -1,
    TCP_BAD_HDR   = -2,
    TCP_BAD_CKSUM = -3,
    TCP_TOO_SHORT = -4,
};

typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seqno;
    uint32_t ackno;
    uint8_t data_offset; // tcp header len in 32-bit words
    uint8_t flags; // fin/syn/rst/psh/ack
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent_ptr;
    const uint8_t *options;
    unsigned options_len;
    const uint8_t *payload;
    unsigned payload_len;
} tcp_hdr_t;

// parse raw tcp datagram into header fields and payload ptrs
int tcp_parse(const uint8_t *seg, unsigned seg_len, tcp_hdr_t *hdr);

// build raw tcp datagram from header plus payload bytes
int tcp_build(const tcp_hdr_t *hdr, const uint8_t *payload, unsigned payload_len,
              uint8_t *out, unsigned out_size);

// compute tcp checksum using part of the ipv4 header
uint16_t tcp_checksum(uint32_t src_ip, uint32_t dst_ip,
                      const uint8_t *tcp_seg, unsigned tcp_len);

#endif
