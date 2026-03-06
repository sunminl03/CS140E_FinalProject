// Test 3: FCS (Frame Check Sequence) verification
// Tests that corrupted frames are detected

#include "rpi.h"
#include "../../hdlc_framing.h"

void notmain(void) {
    printk("=== Test 3: FCS Verification ===\n\n");
    
    // Test case 1: Valid frame should decode successfully
    {
        const uint8_t payload[] = "Test payload";
        unsigned payload_len = strlen((const char *)payload);
        uint8_t frame[256];
        uint8_t decoded[256];
        
        printk("Test 1: Valid frame\n");
        
        int frame_len = hdlc_encode(payload, payload_len, frame, sizeof(frame));
        if (frame_len < 0) {
            printk("  ERROR: Encoding failed\n");
            return;
        }
        
        int decoded_len = hdlc_decode(frame, frame_len, decoded, sizeof(decoded));
        if (decoded_len == payload_len) {
            printk("  SUCCESS: Valid frame decoded correctly\n\n");
        } else {
            printk("  ERROR: Valid frame failed to decode\n\n");
        }
    }
    
    // Test case 2: Corrupted payload should be detected
    {
        const uint8_t payload[] = "Test payload";
        unsigned payload_len = strlen((const char *)payload);
        uint8_t frame[256];
        uint8_t decoded[256];
        
        printk("Test 2: Corrupted payload\n");
        
        int frame_len = hdlc_encode(payload, payload_len, frame, sizeof(frame));
        if (frame_len < 0) {
            printk("  ERROR: Encoding failed\n");
            return;
        }
        
        // Corrupt a byte in the payload (skip flags)
        if (frame_len > 4) {
            frame[1] ^= 0x01;  // Flip a bit in the payload
        }
        
        int decoded_len = hdlc_decode(frame, frame_len, decoded, sizeof(decoded));
        if (decoded_len == HDLC_FCS_ERROR) {
            printk("  SUCCESS: Corrupted payload detected by FCS\n\n");
        } else {
            printk("  ERROR: Corrupted payload not detected (got %d)\n\n", decoded_len);
        }
    }
    
    // Test case 3: Corrupted FCS should be detected
    {
        const uint8_t payload[] = "Test payload";
        unsigned payload_len = strlen((const char *)payload);
        uint8_t frame[256];
        uint8_t decoded[256];
        
        printk("Test 3: Corrupted FCS\n");
        
        int frame_len = hdlc_encode(payload, payload_len, frame, sizeof(frame));
        if (frame_len < 0) {
            printk("  ERROR: Encoding failed\n");
            return;
        }
        
        // Corrupt the last byte before the closing flag (FCS)
        if (frame_len > 2) {
            frame[frame_len - 2] ^= 0x01;  // Flip a bit in FCS
        }
        
        int decoded_len = hdlc_decode(frame, frame_len, decoded, sizeof(decoded));
        if (decoded_len == HDLC_FCS_ERROR) {
            printk("  SUCCESS: Corrupted FCS detected\n\n");
        } else {
            printk("  ERROR: Corrupted FCS not detected (got %d)\n\n", decoded_len);
        }
    }
    
    // Test case 4: Missing FCS bytes should be detected
    {
        const uint8_t payload[] = "Test";
        unsigned payload_len = strlen((const char *)payload);
        uint8_t frame[256];
        uint8_t decoded[256];
        
        printk("Test 4: Incomplete frame (missing FCS)\n");
        
        int frame_len = hdlc_encode(payload, payload_len, frame, sizeof(frame));
        if (frame_len < 0) {
            printk("  ERROR: Encoding failed\n");
            return;
        }
        
        // Remove last byte (part of FCS) and closing flag
        frame_len -= 2;
        
        int decoded_len = hdlc_decode(frame, frame_len, decoded, sizeof(decoded));
        if (decoded_len < 0 && decoded_len != HDLC_FCS_ERROR) {
            printk("  SUCCESS: Incomplete frame detected (got %d)\n\n", decoded_len);
        } else if (decoded_len == HDLC_FCS_ERROR) {
            printk("  SUCCESS: Incomplete frame detected as FCS error\n\n");
        } else {
            printk("  ERROR: Incomplete frame not detected (got %d)\n\n", decoded_len);
        }
    }
    
    printk("=== Test 3 Complete ===\n");
}
