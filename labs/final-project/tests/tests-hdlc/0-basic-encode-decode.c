// Test 0: Basic encode/decode round-trip
// Tests that encoding and decoding preserves the original payload

#include "rpi.h"
#include "../../hdlc_framing.h"

void notmain(void) {
    printk("=== Test 0: Basic Encode/Decode ===\n\n");
    
    // Test case 1: Simple ASCII string
    {
        const uint8_t payload[] = "Hello, World!";
        unsigned payload_len = strlen((const char *)payload);
        uint8_t frame[256];
        uint8_t decoded[256];
        
        printk("Test 1: Simple string\n");
        printk("  Original: '%s' (%d bytes)\n", payload, payload_len);
        
        int frame_len = hdlc_encode(payload, payload_len, frame, sizeof(frame));
        if (frame_len < 0) {
            printk("  ERROR: Encoding failed: %d\n", frame_len);
            return;
        }
        printk("  Encoded: %d bytes\n", frame_len);
        
        int decoded_len = hdlc_decode(frame, frame_len, decoded, sizeof(decoded));
        if (decoded_len < 0) {
            printk("  ERROR: Decoding failed: %d\n", decoded_len);
            return;
        }
        
        if (decoded_len != payload_len) {
            printk("  ERROR: Length mismatch: expected %d, got %d\n", payload_len, decoded_len);
            return;
        }
        
        if (memcmp(payload, decoded, payload_len) != 0) {
            printk("  ERROR: Data mismatch\n");
            return;
        }
        
        printk("  SUCCESS: Round-trip preserved data\n\n");
    }
    
    // Test case 2: Binary data
    {
        const uint8_t payload[] = {0x00, 0x01, 0x02, 0x7E, 0x7D, 0xFF, 0xFE};
        unsigned payload_len = sizeof(payload);
        uint8_t frame[256];
        uint8_t decoded[256];
        
        printk("Test 2: Binary data with special bytes\n");
        printk("  Original: ");
        for (unsigned i = 0; i < payload_len; i++) {
            printk("%x ", payload[i]);
        }
        printk("(%d bytes)\n", payload_len);
        
        int frame_len = hdlc_encode(payload, payload_len, frame, sizeof(frame));
        if (frame_len < 0) {
            printk("  ERROR: Encoding failed: %d\n", frame_len);
            return;
        }
        printk("  Encoded: %d bytes\n", frame_len);
        
        int decoded_len = hdlc_decode(frame, frame_len, decoded, sizeof(decoded));
        if (decoded_len < 0) {
            printk("  ERROR: Decoding failed: %d\n", decoded_len);
            return;
        }
        
        if (decoded_len != payload_len) {
            printk("  ERROR: Length mismatch: expected %d, got %d\n", payload_len, decoded_len);
            return;
        }
        
        if (memcmp(payload, decoded, payload_len) != 0) {
            printk("  ERROR: Data mismatch\n");
            printk("  Decoded: ");
            for (unsigned i = 0; i < decoded_len; i++) {
                printk("%x ", decoded[i]);
            }
            printk("\n");
            return;
        }
        
        printk("  SUCCESS: Round-trip preserved binary data\n\n");
    }
    
    // Test case 3: Empty payload (should fail)
    {
        uint8_t frame[256];
        int frame_len = hdlc_encode(NULL, 0, frame, sizeof(frame));
        if (frame_len >= 0) {
            printk("Test 3: Empty payload - ERROR: Should have failed\n");
        } else {
            printk("Test 3: Empty payload - SUCCESS: Correctly rejected\n\n");
        }
    }
    
    printk("=== Test 0 Complete ===\n");
}
