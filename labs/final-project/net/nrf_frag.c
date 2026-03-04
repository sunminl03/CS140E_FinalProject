#include "nrf_frag.h"
#include "../../14-nrf-networking/code/nrf.h"
#include "../../14-nrf-networking/code/nrf-hw-support.h"
#include "timeout.h"

void nrf_frag_init(nrf_frag_t *f, nrf_t *nrf) {
    assert(f);
    assert(nrf);
    
    f->nrf = nrf;
    f->next_msg_id = 0;
    
    for (unsigned i = 0; i < REASM_SLOTS; i++) {
        f->reasm_slots[i].active = 0;
        f->reasm_slots[i].pbuf = NULL;
    }
}

// Helper: find or allocate a reassembly slot
static reasm_slot_t* find_reasm_slot(nrf_frag_t *f, uint8_t msg_id) {
    // First, try to find existing slot for this msg_id
    for (unsigned i = 0; i < REASM_SLOTS; i++) {
        if (f->reasm_slots[i].active && f->reasm_slots[i].msg_id == msg_id) {
            return &f->reasm_slots[i];
        }
    }
    
    // Find free slot
    for (unsigned i = 0; i < REASM_SLOTS; i++) {
        if (!f->reasm_slots[i].active) {
            reasm_slot_t *slot = &f->reasm_slots[i];
            slot->active = 1;
            slot->msg_id = msg_id;
            slot->frags_received = 0;
            slot->start_usec = timer_get_usec();
            memset(slot->received_mask, 0, sizeof(slot->received_mask));
            slot->pbuf = pbuf_alloc();
            if (!slot->pbuf) {
                slot->active = 0;
                return NULL;  // Out of pbufs
            }
            slot->pbuf->len = 0;
            return slot;
        }
    }
    
    return NULL;  // No free slots
}

// Helper: check if fragment was already received
static int frag_received(reasm_slot_t *slot, uint8_t frag_seq) {
    if (frag_seq >= 64)
        return 0;
    unsigned byte_idx = frag_seq / 8;
    unsigned bit_idx = frag_seq % 8;
    return (slot->received_mask[byte_idx] >> bit_idx) & 1;
}

// Helper: mark fragment as received
static void mark_frag_received(reasm_slot_t *slot, uint8_t frag_seq) {
    if (frag_seq >= 64)
        return;
    unsigned byte_idx = frag_seq / 8;
    unsigned bit_idx = frag_seq % 8;
    slot->received_mask[byte_idx] |= (1 << bit_idx);
}

int nrf_frag_tx(nrf_frag_t *f, uint32_t txaddr, pbuf_t *p) {
    assert(f);
    assert(p);
    
    if (p->len == 0)
        return 0;
    
    if (p->len > NRF_FRAG_MAX_MSG) {
        output("ERROR: message too large: %d bytes (max %d)\n", 
               p->len, NRF_FRAG_MAX_MSG);
        return -1;
    }
    
    uint8_t msg_id = f->next_msg_id++;
    unsigned total_frags = (p->len + NRF_FRAG_PAYLOAD_MAX - 1) / NRF_FRAG_PAYLOAD_MAX;
    
    if (total_frags > NRF_FRAG_MAX_FRAGS) {
        output("ERROR: too many fragments: %d (max %d)\n", 
               total_frags, NRF_FRAG_MAX_FRAGS);
        return -1;
    }
    
    unsigned offset = 0;
    int frags_sent = 0;
    
    for (unsigned frag_seq = 0; frag_seq < total_frags; frag_seq++) {
        // Calculate payload size for this fragment
        unsigned remaining = p->len - offset;
        unsigned payload_len = remaining < NRF_FRAG_PAYLOAD_MAX ? remaining : NRF_FRAG_PAYLOAD_MAX;
        
        // Build fragment header
        nrf_frag_hdr_t hdr;
        hdr.msg_id = msg_id;
        hdr.frag_seq = frag_seq;
        hdr.total_frags = total_frags;
        hdr.payload_len = payload_len;
        
        // Build 32-byte NRF packet: header + payload
        // Note: We must copy into a contiguous 32-byte buffer for NRF hardware,
        // but we're reading directly from pbuf (no intermediate copies of the
        // full message). The zero-copy property is that we don't copy the
        // entire message - we read slices as needed.
        uint8_t nrf_pkt[32];
        memcpy(nrf_pkt, &hdr, NRF_FRAG_HDR_SZ);
        memcpy(nrf_pkt + NRF_FRAG_HDR_SZ, p->data + offset, payload_len);
        
        // Send via NRF (32 bytes total)
        int ret = nrf_tx_send_ack(f->nrf, txaddr, nrf_pkt, 32);
        if (ret < 0) {
            output("ERROR: failed to send fragment %d/%d\n", frag_seq, total_frags);
            return -1;
        }
        
        offset += payload_len;
        frags_sent++;
    }
    
    return frags_sent;
}

pbuf_t* nrf_frag_rx(nrf_frag_t *f) {
    assert(f);
    
    // Clean up timeouts first
    nrf_frag_cleanup_timeouts(f);
    
    // Pull packets from NRF hardware
    nrf_get_pkts(f->nrf);
    
    // Check if we have a complete 32-byte packet
    if (cq_nelem(&f->nrf->recvq) < 32)
        return NULL;
    
    // Read 32-byte fragment
    uint8_t nrf_pkt[32];
    cq_pop_n(&f->nrf->recvq, nrf_pkt, 32);
    
    // Parse fragment header
    nrf_frag_hdr_t *hdr = (nrf_frag_hdr_t*)nrf_pkt;
    uint8_t msg_id = hdr->msg_id;
    uint8_t frag_seq = hdr->frag_seq;
    uint8_t total_frags = hdr->total_frags;
    uint8_t payload_len = hdr->payload_len;
    
    // Validate header
    if (total_frags == 0 || total_frags > NRF_FRAG_MAX_FRAGS) {
        output("ERROR: invalid total_frags: %d\n", total_frags);
        return NULL;
    }
    
    if (frag_seq >= total_frags) {
        output("ERROR: frag_seq %d >= total_frags %d\n", frag_seq, total_frags);
        return NULL;
    }
    
    if (payload_len == 0 || payload_len > NRF_FRAG_PAYLOAD_MAX) {
        output("ERROR: invalid payload_len: %d\n", payload_len);
        return NULL;
    }
    
    // Find or allocate reassembly slot
    reasm_slot_t *slot = find_reasm_slot(f, msg_id);
    if (!slot) {
        output("ERROR: no reassembly slot available\n");
        return NULL;
    }
    
    // Check if we already received this fragment
    if (frag_received(slot, frag_seq)) {
        // Duplicate fragment - ignore
        return NULL;
    }
    
    // Validate slot matches this message
    if (slot->total_frags == 0) {
        // First fragment - initialize slot
        slot->total_frags = total_frags;
    } else if (slot->total_frags != total_frags) {
        output("ERROR: total_frags mismatch: slot=%d, frag=%d\n", 
               slot->total_frags, total_frags);
        return NULL;
    }
    
    // Calculate offset in reassembly buffer
    unsigned offset = frag_seq * NRF_FRAG_PAYLOAD_MAX;
    
    // Ensure pbuf is large enough
    unsigned needed_len = offset + payload_len;
    if (needed_len > PBUF_DATA_SZ) {
        output("ERROR: reassembly buffer overflow\n");
        slot->active = 0;
        if (slot->pbuf)
            pbuf_free(slot->pbuf);
        return NULL;
    }
    
    // Copy payload into reassembly buffer (only place we copy payload!)
    memcpy(slot->pbuf->data + offset, nrf_pkt + NRF_FRAG_HDR_SZ, payload_len);
    
    // Update length if this extends the buffer
    if (needed_len > slot->pbuf->len)
        slot->pbuf->len = needed_len;
    
    // Mark fragment as received
    mark_frag_received(slot, frag_seq);
    slot->frags_received++;
    
    // Check if we have all fragments
    if (slot->frags_received == slot->total_frags) {
        // Complete message!
        pbuf_t *complete = slot->pbuf;
        slot->active = 0;
        slot->pbuf = NULL;
        return complete;
    }
    
    return NULL;  // Still waiting for more fragments
}

void nrf_frag_cleanup_timeouts(nrf_frag_t *f) {
    assert(f);
    
    uint32_t now = timer_get_usec();
    
    for (unsigned i = 0; i < REASM_SLOTS; i++) {
        reasm_slot_t *slot = &f->reasm_slots[i];
        if (!slot->active)
            continue;
        
        // Check timeout
        uint32_t elapsed = now - slot->start_usec;
        if (elapsed > REASM_TIMEOUT_USEC) {
            output("WARNING: reassembly timeout for msg_id %d (%d/%d fragments)\n",
                   slot->msg_id, slot->frags_received, slot->total_frags);
            slot->active = 0;
            if (slot->pbuf) {
                pbuf_free(slot->pbuf);
                slot->pbuf = NULL;
            }
        }
    }
}
