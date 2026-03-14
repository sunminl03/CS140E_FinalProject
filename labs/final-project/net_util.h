#ifndef NET_UTIL_H
#define NET_UTIL_H

#include <stdint.h>

static inline uint16_t get_be16(const uint8_t *p) {
    return ((uint16_t)p[0] << 8) | p[1];
}

static inline uint32_t get_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  |
           ((uint32_t)p[3]);
}

static inline void put_be16(uint8_t *p, uint16_t x) {
    p[0] = (x >> 8) & 0xFF;
    p[1] = x & 0xFF;
}

static inline void put_be32(uint8_t *p, uint32_t x) {
    p[0] = (x >> 24) & 0xFF;
    p[1] = (x >> 16) & 0xFF;
    p[2] = (x >> 8) & 0xFF;
    p[3] = x & 0xFF;
}

#endif
