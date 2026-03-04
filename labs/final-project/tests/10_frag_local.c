// Test fragmentation and reassembly locally (no network stack)
// Sends a message, fragments it, reassembles it, verifies CRC32

#include "rpi.h"
#include "../../14-nrf-networking/code/nrf-test.h"
#include "../net/nrf_frag.h"
#include "../net/pbuf.h"

// Simple CRC32 implementation for testing
static uint32_t crc32_table[256];

static void crc32_init(void) {
    static int initialized = 0;
    if (initialized)
        return;
    
    for (unsigned i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xedb88320;
            else
                crc >>= 1;
        }
        crc32_table[i] = crc;
    }
    initialized = 1;
}

static uint32_t crc32(const void *data, unsigned len) {
    crc32_init();
    const uint8_t *p = (const uint8_t*)data;
    uint32_t crc = 0xffffffff;
    
    for (unsigned i = 0; i < len; i++) {
        crc = (crc >> 8) ^ crc32_table[(crc ^ p[i]) & 0xff];
    }
    
    return crc ^ 0xffffffff;
}

// Test fragmentation and reassembly
static void test_frag_reasm(nrf_t *server, nrf_t *client, unsigned msg_size) {
    trace("Testing fragmentation/reassembly with %d byte message\n", msg_size);
    
    // Initialize fragmentation contexts
    nrf_frag_t server_frag, client_frag;
    nrf_frag_init(&server_frag, server);
    nrf_frag_init(&client_frag, client);
    
    // Initialize pbuf pool
    pbuf_init();
    
    // Create test message with CRC32
    pbuf_t *tx_pbuf = pbuf_alloc();
    if (!tx_pbuf)
        panic("Failed to allocate tx pbuf\n");
    
    // Fill with test data
    for (unsigned i = 0; i < msg_size; i++) {
        tx_pbuf->data[i] = (uint8_t)(i ^ 0xaa);
    }
    tx_pbuf->len = msg_size;
    
    // Calculate CRC32
    uint32_t tx_crc = crc32(tx_pbuf->data, msg_size);
    trace("TX message: %d bytes, CRC32=0x%08x\n", msg_size, tx_crc);
    
    // Fragment and send
    unsigned client_addr = client->rxaddr;
    int frags_sent = nrf_frag_tx(&server_frag, client_addr, tx_pbuf);
    if (frags_sent < 0)
        panic("Failed to fragment and send\n");
    
    trace("Sent %d fragments\n", frags_sent);
    pbuf_free(tx_pbuf);
    
    // Receive and reassemble
    timeout_t t = timeout_start();
    pbuf_t *rx_pbuf = NULL;
    
    while (timeout_get_usec(&t) < 5000000) {  // 5 second timeout
        rx_pbuf = nrf_frag_rx(&client_frag);
        if (rx_pbuf)
            break;
        
        // Clean up timeouts
        nrf_frag_cleanup_timeouts(&client_frag);
        
        delay_ms(10);  // Small delay
    }
    
    if (!rx_pbuf)
        panic("Failed to receive and reassemble message\n");
    
    trace("Received and reassembled: %d bytes\n", rx_pbuf->len);
    
    // Verify length
    if (rx_pbuf->len != msg_size) {
        panic("Length mismatch: expected %d, got %d\n", msg_size, rx_pbuf->len);
    }
    
    // Verify CRC32
    uint32_t rx_crc = crc32(rx_pbuf->data, rx_pbuf->len);
    if (rx_crc != tx_crc) {
        panic("CRC32 mismatch: expected 0x%08x, got 0x%08x\n", tx_crc, rx_crc);
    }
    
    trace("SUCCESS: CRC32 match after reassembly!\n");
    
    pbuf_free(rx_pbuf);
}

void notmain(void) {
    kmalloc_init_mb(1);
    
    // Configure NRF for 32-byte packets (fragmentation layer)
    unsigned nbytes = 32;
    
    trace("Configuring server=[%x] with %d byte packets\n", server_addr, nbytes);
    nrf_t *server = server_mk_ack(server_addr, nbytes);
    nrf_dump("server config:\n", server);
    
    trace("Configuring client=[%x] with %d byte packets\n", client_addr, nbytes);
    nrf_t *client = client_mk_ack(client_addr, nbytes);
    nrf_dump("client config:\n", client);
    
    // Compatibility check
    if (!nrf_compat(client, server))
        panic("NRF configuration not compatible\n");
    
    // Reset stats
    nrf_stat_start(server);
    nrf_stat_start(client);
    
    // Test with different message sizes
    trace("\n=== Test 1: Small message (100 bytes) ===\n");
    test_frag_reasm(server, client, 100);
    
    trace("\n=== Test 2: Medium message (500 bytes) ===\n");
    test_frag_reasm(server, client, 500);
    
    trace("\n=== Test 3: Large message (1500 bytes) ===\n");
    test_frag_reasm(server, client, 1500);
    
    trace("\n=== All tests passed! ===\n");
    
    // Print stats
    nrf_stat_print(server, "server: done");
    nrf_stat_print(client, "client: done");
}
