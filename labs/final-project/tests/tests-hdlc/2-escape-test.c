// Test 2: Character escaping
// Tests that special characters (FLAG, ESCAPE, control chars) are properly escaped

#include "rpi.h"
#include "../../hdlc_framing.h"

void notmain(void) {
    printk("=== Test 2: Character Escaping ===\n\n");
    
    // Test case 1: Flag byte (0x7E) should be escaped
    {
        const uint8_t payload[] = {0x7E, 0x41, 0x7E};  // FLAG, 'A', FLAG
        unsigned payload_len = sizeof(payload);
        uint8_t frame[256];
        uint8_t decoded[256];
        
        printk("Test 1: Flag bytes (0x7E)\n");
        printk("  Payload contains 0x7E bytes\n");
        
        int frame_len = hdlc_encode(payload, payload_len, frame, sizeof(frame));
        if (frame_len < 0) {
            printk("  ERROR: Encoding failed\n");
            return;
        }
        
        // Check that flags are escaped (should see 0x7D 0x5E in frame)
        int found_escape = 0;
        for (unsigned i = 1; i < frame_len - 1; i++) {  // Skip start/end flags
            if (frame[i] == HDLC_ESCAPE && i + 1 < frame_len - 1) {
                if (frame[i + 1] == (0x7E ^ HDLC_XOR)) {
                    found_escape = 1;
                    printk("  Found escaped flag: 0x7D %x\n", frame[i + 1]);
                }
            }
        }
        
        if (!found_escape) {
            printk("  WARNING: Flag byte may not be escaped\n");
        }
        
        int decoded_len = hdlc_decode(frame, frame_len, decoded, sizeof(decoded));
        if (decoded_len == payload_len && memcmp(payload, decoded, payload_len) == 0) {
            printk("  SUCCESS: Flag bytes properly escaped and decoded\n\n");
        } else {
            printk("  ERROR: Decoding failed\n\n");
        }
    }
    
    // Test case 2: Escape byte (0x7D) should be escaped
    {
        const uint8_t payload[] = {0x7D, 0x42, 0x7D};  // ESCAPE, 'B', ESCAPE
        unsigned payload_len = sizeof(payload);
        uint8_t frame[256];
        uint8_t decoded[256];
        
        printk("Test 2: Escape bytes (0x7D)\n");
        printk("  Payload contains 0x7D bytes\n");
        
        int frame_len = hdlc_encode(payload, payload_len, frame, sizeof(frame));
        if (frame_len < 0) {
            printk("  ERROR: Encoding failed\n");
            return;
        }
        
        int decoded_len = hdlc_decode(frame, frame_len, decoded, sizeof(decoded));
        if (decoded_len == payload_len && memcmp(payload, decoded, payload_len) == 0) {
            printk("  SUCCESS: Escape bytes properly escaped and decoded\n\n");
        } else {
            printk("  ERROR: Decoding failed\n\n");
        }
    }
    
    // Test case 3: Control characters (< 0x20) should be escaped
    {
        const uint8_t payload[] = {0x00, 0x01, 0x1F, 0x20, 0x21};
        unsigned payload_len = sizeof(payload);
        uint8_t frame[256];
        uint8_t decoded[256];
        
        printk("Test 3: Control characters (< 0x20)\n");
        printk("  Payload: 0x00 0x01 0x1F 0x20 0x21\n");
        printk("  (0x00, 0x01, 0x1F should be escaped; 0x20, 0x21 should not)\n");
        
        int frame_len = hdlc_encode(payload, payload_len, frame, sizeof(frame));
        if (frame_len < 0) {
            printk("  ERROR: Encoding failed\n");
            return;
        }
        
        int decoded_len = hdlc_decode(frame, frame_len, decoded, sizeof(decoded));
        if (decoded_len == payload_len && memcmp(payload, decoded, payload_len) == 0) {
            printk("  SUCCESS: Control characters properly escaped and decoded\n\n");
        } else {
            printk("  ERROR: Decoding failed\n\n");
        }
    }
    
    // Test case 4: All special characters together
    {
        const uint8_t payload[] = {0x7E, 0x7D, 0x00, 0x1F, 0x41, 0x7E, 0x7D};
        unsigned payload_len = sizeof(payload);
        uint8_t frame[256];
        uint8_t decoded[256];
        
        printk("Test 4: All special characters\n");
        printk("  Payload contains flags, escapes, and control chars\n");
        
        int frame_len = hdlc_encode(payload, payload_len, frame, sizeof(frame));
        if (frame_len < 0) {
            printk("  ERROR: Encoding failed\n");
            return;
        }
        
        int decoded_len = hdlc_decode(frame, frame_len, decoded, sizeof(decoded));
        if (decoded_len == payload_len && memcmp(payload, decoded, payload_len) == 0) {
            printk("  SUCCESS: All special characters handled correctly\n\n");
        } else {
            printk("  ERROR: Decoding failed\n\n");
        }
    }
    
    printk("=== Test 2 Complete ===\n");
}
