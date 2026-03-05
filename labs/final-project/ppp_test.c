// Test program for PPP frame parsing.
// This is the main entry point that initializes UART and runs the PPP parser.

#include "rpi.h"

// Declare the PPP parser function
void ppp_frame_parser_loop(void);

void notmain(void) {
    // Initialize UART at 115200 8N1
    uart_init();
    
    // Run the PPP frame parser
    ppp_frame_parser_loop();
}
