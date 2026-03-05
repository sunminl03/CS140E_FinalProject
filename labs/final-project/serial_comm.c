// Simple serial communication demo for the final project.
// Reuses the UART implementation from labs/7-uart to
// talk between the Pi and the laptop over USB-serial.
//
// Behavior:
//  - Initializes UART at 115200 8N1.
//  - Prints a startup banner.
//  - Echoes every character it receives, and on newline
//    sends back a simple "you typed: ..." line.

#include "rpi.h"

// Read a line (up to max-1 chars) from UART, echoing using printk.
// Line ends on '\n' or '\r'. Buffer is always NUL-terminated.
static int uart_readline(char *buf, int max) {
    int i = 0;
    while (i < max - 1) {
        int c = uart_get8();

        // handle backspace / delete locally
        if (c == 0x08 || c == 0x7f) {
            if (i > 0) {
                // erase character on the terminal
                printk("\b \b");
                i--;
            }
            continue;
        }

        // newline / return: finish the line
        if (c == '\r' || c == '\n') {
            // echo as newline; printk will map to CRLF
            printk("\n");
            buf[i++] = '\n';
            break;
        }

        // normal character: echo and store
        printk("%c", c);
        buf[i++] = (char)c;
    }
    buf[i] = 0;
    return i;
}

void notmain(void) {
    uart_init();

    printk("\n");
    printk("========================================\n");
    printk(" Final Project: Pi <-> Laptop Serial Demo\n");
    printk(" 115200 8N1, no flow control\n");
    printk(" Type a line on the laptop terminal and hit Enter.\n");
    printk(" The Pi will echo and then reply with \"you typed: ...\".\n");
    printk("========================================\n\n");

    char buf[128];

    while (1) {
        printk("pi> ");
        int n = uart_readline(buf, sizeof buf);
        if (n <= 0)
            continue;

        printk("you typed: %s", buf);
    }
}

