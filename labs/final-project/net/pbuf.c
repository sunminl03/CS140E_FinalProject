#include "pbuf.h"

static pbuf_t pbuf_pool[PBUF_POOL_N];
static unsigned pbuf_pool_initialized = 0;

void pbuf_init(void) {
    if (pbuf_pool_initialized)
        return;
    
    for (unsigned i = 0; i < PBUF_POOL_N; i++) {
        pbuf_t *p = &pbuf_pool[i];
        // Initialize data pointer to start of payload area (after headroom)
        p->data = p->raw + PBUF_HEADROOM;
        p->len = 0;
        p->refcnt = 0;
    }
    
    pbuf_pool_initialized = 1;
}

pbuf_t* pbuf_alloc(void) {
    if (!pbuf_pool_initialized)
        pbuf_init();
    
    for (unsigned i = 0; i < PBUF_POOL_N; i++) {
        pbuf_t *p = &pbuf_pool[i];
        if (p->refcnt == 0) {
            // Reset to initial state
            p->data = p->raw + PBUF_HEADROOM;
            p->len = 0;
            p->refcnt = 1;
            return p;
        }
    }
    
    return NULL;  // Pool exhausted
}

void pbuf_free(pbuf_t *p) {
    if (!p)
        return;
    
    assert(p->refcnt > 0);
    p->refcnt = 0;
    // Don't zero out data - just mark as free
    // Next alloc will reset it
}
