// PPP frame parser for the final project.
// Implements basic PPP frame detection:
//  - Detects 0x7E (PPP flag byte) as frame start
//  - Collects bytes until next 0x7E (frame end)
//  - Treats everything between flags as one packet
//
// Note: This is a minimal implementation. Full PPP also handles:
//  - Byte stuffing (0x7D escape sequences)
//  - FCS (Frame Check Sequence / CRC)
//  - Protocol field parsing

#include "rpi.h"

// Maximum frame size (excluding flags)
#define PPP_MAX_FRAME 1500

// PPP frame structure:
//  [0x7E] [protocol] [data...] [FCS] [0x7E]
//  flag   (2 bytes)  (variable) (2 bytes) flag

// State for frame parsing
typedef struct {
    int in_frame;           // 1 if we're currently collecting a frame
    int frame_len;           // number of bytes collected so far
    uint8_t frame_buf[PPP_MAX_FRAME];  // buffer for current frame
} ppp_state_t;

static ppp_state_t ppp_state = {0};

// Reset frame state (call when starting a new frame or on error)
static void ppp_reset(void) {
    ppp_state.in_frame = 0;
    ppp_state.frame_len = 0;
}

// Start collecting a new frame (called when we see 0x7E)
static void ppp_start_frame(void) {
    ppp_reset();
    ppp_state.in_frame = 1;
}

// Add a byte to the current frame
// Returns: 1 if byte was added, 0 if frame is full
static int ppp_add_byte(uint8_t byte) {
    if (!ppp_state.in_frame)
        return 0;
    
    if (ppp_state.frame_len >= PPP_MAX_FRAME) {
        // Frame too large, reset
        printk("PPP: frame too large, resetting\n");
        ppp_reset();
        return 0;
    }
    
    ppp_state.frame_buf[ppp_state.frame_len++] = byte;
    return 1;
}

// Process a complete frame (called when we see ending 0x7E)
static void ppp_process_frame(void) {
    if (!ppp_state.in_frame || ppp_state.frame_len == 0) {
        // Empty frame or not in frame state
        ppp_reset();
        return;
    }
    
    // Print frame info
    printk("PPP: received frame (%d bytes): ", ppp_state.frame_len);
    
    // Print first few bytes as hex for debugging
    int print_len = ppp_state.frame_len < 16 ? ppp_state.frame_len : 16;
    for (int i = 0; i < print_len; i++) {
        printk("%02x ", ppp_state.frame_buf[i]);
    }
    if (ppp_state.frame_len > 16)
        printk("...");
    printk("\n");
    
    // TODO: Parse protocol field, validate FCS, handle payload
    // For now, we just detect and report frames
    
    ppp_reset();
}

// Process one byte from UART in the context of PPP frame parsing
// Returns: 1 if a complete frame was detected, 0 otherwise
int ppp_process_byte(uint8_t byte) {
    if (byte == 0x7E) {
        // Flag byte detected
        if (ppp_state.in_frame) {
            // This is an ending flag - process the frame we collected
            ppp_process_frame();
        } else {
            // This is a starting flag - begin collecting a new frame
            ppp_start_frame();
        }
        return 0;  // Flag byte itself is not part of the frame
    }
    
    // Not a flag byte - add to current frame if we're in one
    if (ppp_state.in_frame) {
        ppp_add_byte(byte);
    }
    // If not in frame, ignore bytes until we see a flag
    
    return 0;
}

// Main PPP frame parser loop
// Continuously reads bytes from UART and processes them as PPP frames
void ppp_frame_parser_loop(void) {
    printk("\n");
    printk("========================================\n");
    printk(" PPP Frame Parser\n");
    printk(" Waiting for PPP frames (0x7E flags)...\n");
    printk("========================================\n\n");
    
    ppp_reset();
    
    while (1) {
        // Block until we get a byte
        uint8_t byte = (uint8_t)uart_get8();
        
        // Process the byte
        ppp_process_byte(byte);
    }
}
