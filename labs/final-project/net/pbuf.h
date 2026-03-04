#ifndef __PBUF_H__
#define __PBUF_H__

#include "rpi.h"

// Packet buffer pool for zero-copy networking
// Headroom allows header prepending without memcpy

enum {
    PBUF_HEADROOM = 64,      // ETH(14)+IP(20)+UDP(8)+slack
    PBUF_DATA_SZ  = 1600,    // >= MTU + ETH header
    PBUF_POOL_N   = 32,      // Number of buffers in pool
};

typedef struct pbuf {
    uint8_t  raw[PBUF_HEADROOM + PBUF_DATA_SZ];
    uint8_t *data;     // start of valid data (points into raw)
    unsigned len;      // length of valid data
    unsigned refcnt;   // 0=free, 1=in use
} pbuf_t;

// Initialize the pbuf pool (call once at startup)
void pbuf_init(void);

// Allocate a pbuf from the pool
// Returns NULL if pool is exhausted
pbuf_t* pbuf_alloc(void);

// Free a pbuf back to the pool
void pbuf_free(pbuf_t *p);

// Prepend header by moving data pointer backward
// This allows zero-copy header addition
static inline void pbuf_push(pbuf_t *p, unsigned hdr_sz) {
    assert(p);
    assert(p->data >= p->raw);
    assert((unsigned)(p->data - p->raw) >= hdr_sz);
    p->data -= hdr_sz;
    p->len += hdr_sz;
}

// Consume header by moving data pointer forward
// This allows zero-copy header removal
static inline void pbuf_pull(pbuf_t *p, unsigned hdr_sz) {
    assert(p);
    assert(p->len >= hdr_sz);
    p->data += hdr_sz;
    p->len -= hdr_sz;
}

// Get pointer to start of data
static inline uint8_t* pbuf_data(pbuf_t *p) {
    assert(p);
    return p->data;
}

// Get length of data
static inline unsigned pbuf_len(pbuf_t *p) {
    assert(p);
    return p->len;
}

// Check if pbuf is free
static inline int pbuf_is_free(pbuf_t *p) {
    return p && p->refcnt == 0;
}

#endif // __PBUF_H__
