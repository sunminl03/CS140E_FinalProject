// Test 4: Edge cases
// Tests edge cases like maximum size, minimum size, etc.

#include "rpi.h"
#include "../../hdlc_framing.h"

void notmain(void) {
    printk("=== Test 4: Edge Cases ===\n\n");
    
    // Test case 1: Single byte payload
    {
        const uint8_t payload[] = {0x41};  // 'A'
        unsigned payload_len = 1;
        uint8_t frame[256];
        uint8_t decoded[256];
        
        printk("Test 1: Single byte payload\n");
        
        int frame_len = hdlc_encode(payload, payload_len, frame, sizeof(frame));
        if (frame_len < 0) {
            printk("  ERROR: Encoding failed\n");
            return;
        }
        
        int decoded_len = hdlc_decode(frame, frame_len, decoded, sizeof(decoded));
        if (decoded_len == payload_len && decoded[0] == payload[0]) {
            printk("  SUCCESS: Single byte handled correctly\n\n");
        } else {
            printk("  ERROR: Single byte failed\n\n");
        }
    }
    
    // Test case 2: Large payload (but within limits)
    {
        uint8_t payload[500];
        for (unsigned i = 0; i < sizeof(payload); i++) {
            payload[i] = (uint8_t)(i & 0xFF);
        }
        unsigned payload_len = sizeof(payload);
        uint8_t frame[HDLC_MAX_FRAME_SIZE];
        uint8_t decoded[HDLC_MAX_FRAME_SIZE];
        
        printk("Test 2: Large payload (%d bytes)\n", payload_len);
        
        int frame_len = hdlc_encode(payload, payload_len, frame, sizeof(frame));
        if (frame_len < 0) {
            printk("  ERROR: Encoding failed: %d\n", frame_len);
            return;
        }
        printk("  Encoded to %d bytes\n", frame_len);
        
        int decoded_len = hdlc_decode(frame, frame_len, decoded, sizeof(decoded));
        if (decoded_len == payload_len && memcmp(payload, decoded, payload_len) == 0) {
            printk("  SUCCESS: Large payload handled correctly\n\n");
        } else {
            printk("  ERROR: Large payload failed\n\n");
        }
    }
    
    // Test case 3: Payload that requires maximum escaping
    {
        uint8_t payload[100];
        // Fill with bytes that need escaping (flags, escapes, control chars)
        for (unsigned i = 0; i < sizeof(payload); i++) {
            payload[i] = (i % 3 == 0) ? 0x7E : (i % 3 == 1) ? 0x7D : 0x00;
        }
        unsigned payload_len = sizeof(payload);
        uint8_t frame[HDLC_MAX_FRAME_SIZE];
        uint8_t decoded[HDLC_MAX_FRAME_SIZE];
        
        printk("Test 3: Payload requiring maximum escaping\n");
        printk("  Payload: %d bytes (all need escaping)\n", payload_len);
        
        int frame_len = hdlc_encode(payload, payload_len, frame, sizeof(frame));
        if (frame_len < 0) {
            printk("  ERROR: Encoding failed: %d\n", frame_len);
            return;
        }
        // Calculate expansion as integer percentage (avoid floating point)
        unsigned expansion_percent = (frame_len * 100) / payload_len;
        printk("  Encoded to %d bytes (expansion: %d percent)\n", 
               frame_len, expansion_percent);
        
        int decoded_len = hdlc_decode(frame, frame_len, decoded, sizeof(decoded));
        if (decoded_len == payload_len && memcmp(payload, decoded, payload_len) == 0) {
            printk("  SUCCESS: Maximum escaping handled correctly\n\n");
        } else {
            printk("  ERROR: Maximum escaping failed\n\n");
        }
    }
    
    // Test case 4: Payload that requires no escaping
    {
        uint8_t payload[100];
        // Fill with bytes that don't need escaping (0x20-0x7D, 0x7F-0xFF)
        for (unsigned i = 0; i < sizeof(payload); i++) {
            payload[i] = 0x41 + (i % 50);  // ASCII letters
        }
        unsigned payload_len = sizeof(payload);
        uint8_t frame[256];
        uint8_t decoded[256];
        
        printk("Test 4: Payload requiring no escaping\n");
        printk("  Payload: %d bytes (none need escaping)\n", payload_len);
        
        int frame_len = hdlc_encode(payload, payload_len, frame, sizeof(frame));
        if (frame_len < 0) {
            printk("  ERROR: Encoding failed\n");
            return;
        }
        // Should be payload + 2 flags + 2 FCS bytes
        unsigned expected_min = payload_len + 2 + 2;
        printk("  Encoded to %d bytes (expected at least %d)\n", frame_len, expected_min);
        
        int decoded_len = hdlc_decode(frame, frame_len, decoded, sizeof(decoded));
        if (decoded_len == payload_len && memcmp(payload, decoded, payload_len) == 0) {
            printk("  SUCCESS: No escaping handled correctly\n\n");
        } else {
            printk("  ERROR: No escaping failed\n\n");
        }
    }
    
    // Test case 5: Buffer too small for encoding
    {
        const uint8_t payload[] = "Test";
        uint8_t frame[5];  // Too small
        
        printk("Test 5: Buffer too small for encoding\n");
        
        int frame_len = hdlc_encode(payload, strlen((const char *)payload), 
                                   frame, sizeof(frame));
        if (frame_len == HDLC_TOO_LARGE) {
            printk("  SUCCESS: Correctly detected buffer too small\n\n");
        } else {
            printk("  ERROR: Should have detected buffer too small (got %d)\n\n", frame_len);
        }
    }
    
    // Test case 6: Buffer too small for decoding
    {
        const uint8_t payload[] = "Test";
        uint8_t frame[256];
        uint8_t decoded[2];  // Too small
        
        printk("Test 6: Decode buffer too small\n");
        
        int frame_len = hdlc_encode(payload, strlen((const char *)payload), 
                                   frame, sizeof(frame));
        if (frame_len < 0) {
            printk("  ERROR: Encoding failed\n");
            return;
        }
        
        int decoded_len = hdlc_decode(frame, frame_len, decoded, sizeof(decoded));
        if (decoded_len == HDLC_TOO_LARGE) {
            printk("  SUCCESS: Correctly detected decode buffer too small\n\n");
        } else {
            printk("  ERROR: Should have detected buffer too small (got %d)\n\n", decoded_len);
        }
    }
    
    printk("=== Test 4 Complete ===\n");
}
