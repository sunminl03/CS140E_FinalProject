#ifndef __NRF_FRAG_H__
#define __NRF_FRAG_H__

#include "rpi.h"
#include "../../14-nrf-networking/code/nrf.h"
#include "pbuf.h"

// Fragment header (4 bytes) + payload (28 bytes) = 32 bytes NRF packet
typedef struct __attribute__((packed)) {
    uint8_t msg_id;       // rolling message ID (0-255)
    uint8_t frag_seq;     // fragment index within message (0-63)
    uint8_t total_frags;  // total fragments for this message (1-64)
    uint8_t payload_len;  // payload bytes in THIS fragment (1-28)
} nrf_frag_hdr_t;

enum {
    NRF_FRAG_HDR_SZ = 4,
    NRF_FRAG_PAYLOAD_MAX = 28,  // 32 - 4 = 28 bytes
    NRF_FRAG_MAX_FRAGS = 64,
    NRF_FRAG_MAX_MSG = NRF_FRAG_MAX_FRAGS * NRF_FRAG_PAYLOAD_MAX,  // 1792 bytes
    REASM_TIMEOUT_USEC = 2000000,  // 2 seconds timeout
    REASM_SLOTS = 4,  // Number of concurrent reassembly slots
};

// Reassembly state for a single message
typedef struct {
    uint8_t  msg_id;
    uint8_t  total_frags;
    uint8_t  received_mask[8];  // bitmask for up to 64 fragments (8 bytes = 64 bits)
    unsigned frags_received;
    uint32_t start_usec;        // timeout tracking
    pbuf_t  *pbuf;              // reassembly buffer
    int      active;            // 1 if slot is in use
} reasm_slot_t;

// Fragmentation context
typedef struct {
    nrf_t *nrf;
    uint8_t next_msg_id;  // Next message ID to use
    reasm_slot_t reasm_slots[REASM_SLOTS];
} nrf_frag_t;

// Initialize fragmentation context
void nrf_frag_init(nrf_frag_t *f, nrf_t *nrf);

// Fragment and send a pbuf over NRF
// Returns number of fragments sent, or -1 on error
int nrf_frag_tx(nrf_frag_t *f, uint32_t txaddr, pbuf_t *p);

// Receive fragments from NRF and reassemble into pbuf
// Returns pbuf if complete message received, NULL otherwise
// Caller must free the returned pbuf
pbuf_t* nrf_frag_rx(nrf_frag_t *f);

// Clean up timed-out reassembly slots
void nrf_frag_cleanup_timeouts(nrf_frag_t *f);

#endif // __NRF_FRAG_H__
