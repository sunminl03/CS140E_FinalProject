// Test 5: Error handling
// Tests various error conditions

#include "rpi.h"
#include "../../hdlc_framing.h"

void notmain(void) {
    printk("=== Test 5: Error Handling ===\n\n");
    
    // Test case 1: NULL payload pointer
    {
        uint8_t frame[256];
        printk("Test 1: NULL payload pointer\n");
        
        int result = hdlc_encode(NULL, 10, frame, sizeof(frame));
        if (result == HDLC_ERROR) {
            printk("  SUCCESS: NULL payload correctly rejected\n\n");
        } else {
            printk("  ERROR: NULL payload not rejected (got %d)\n\n", result);
        }
    }
    
    // Test case 2: NULL frame buffer
    {
        const uint8_t payload[] = "Test";
        printk("Test 2: NULL frame buffer\n");
        
        int result = hdlc_encode(payload, strlen((const char *)payload), NULL, 256);
        if (result == HDLC_ERROR) {
            printk("  SUCCESS: NULL frame buffer correctly rejected\n\n");
        } else {
            printk("  ERROR: NULL frame buffer not rejected (got %d)\n\n", result);
        }
    }
    
    // Test case 3: NULL decode buffer
    {
        const uint8_t payload[] = "Test";
        uint8_t frame[256];
        printk("Test 3: NULL decode buffer\n");
        
        int frame_len = hdlc_encode(payload, strlen((const char *)payload), 
                                   frame, sizeof(frame));
        if (frame_len < 0) {
            printk("  ERROR: Encoding failed\n");
            return;
        }
        
        int result = hdlc_decode(frame, frame_len, NULL, 256);
        if (result == HDLC_ERROR) {
            printk("  SUCCESS: NULL decode buffer correctly rejected\n\n");
        } else {
            printk("  ERROR: NULL decode buffer not rejected (got %d)\n\n", result);
        }
    }
    
    // Test case 4: Frame too short (no data)
    {
        uint8_t frame[] = {HDLC_FLAG, HDLC_FLAG};  // Just two flags
        uint8_t decoded[256];
        printk("Test 4: Frame too short (no data)\n");
        
        int result = hdlc_decode(frame, sizeof(frame), decoded, sizeof(decoded));
        if (result < 0) {
            printk("  SUCCESS: Too short frame correctly rejected (got %d)\n\n", result);
        } else {
            printk("  ERROR: Too short frame not rejected (got %d)\n\n", result);
        }
    }
    
    // Test case 5: Frame with only flags
    {
        uint8_t frame[] = {HDLC_FLAG, HDLC_FLAG, HDLC_FLAG};
        uint8_t decoded[256];
        printk("Test 5: Frame with only flags\n");
        
        int result = hdlc_decode(frame, sizeof(frame), decoded, sizeof(decoded));
        if (result < 0) {
            printk("  SUCCESS: Flags-only frame correctly rejected (got %d)\n\n", result);
        } else {
            printk("  ERROR: Flags-only frame not rejected (got %d)\n\n", result);
        }
    }
    
    // Test case 6: Incomplete escape sequence
    {
        uint8_t frame[] = {HDLC_FLAG, HDLC_ESCAPE, HDLC_FLAG};  // Escape without following byte
        uint8_t decoded[256];
        printk("Test 6: Incomplete escape sequence\n");
        
        int result = hdlc_decode(frame, sizeof(frame), decoded, sizeof(decoded));
        if (result == HDLC_INCOMPLETE || result == HDLC_ERROR) {
            printk("  SUCCESS: Incomplete escape correctly detected (got %d)\n\n", result);
        } else {
            printk("  ERROR: Incomplete escape not detected (got %d)\n\n", result);
        }
    }
    
    // Test case 7: Frame with invalid structure (no closing flag)
    {
        const uint8_t payload[] = "Test";
        uint8_t frame[256];
        uint8_t decoded[256];
        printk("Test 7: Frame without closing flag\n");
        
        int frame_len = hdlc_encode(payload, strlen((const char *)payload), 
                                   frame, sizeof(frame));
        if (frame_len < 0) {
            printk("  ERROR: Encoding failed\n");
            return;
        }
        
        // Remove closing flag
        frame_len--;
        
        int result = hdlc_decode(frame, frame_len, decoded, sizeof(decoded));
        // Should still work if we have enough data, but FCS might be wrong
        printk("  Result: %d (may be valid if enough data present)\n\n", result);
    }
    
    printk("=== Test 5 Complete ===\n");
}
