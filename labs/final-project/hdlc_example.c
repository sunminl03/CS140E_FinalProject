// Example usage of HDLC-like framing for PPP communication
// This demonstrates how to send and receive framed packets

#include "rpi.h"
#include "hdlc_framing.h"

void notmain(void) {
    uart_init();
    
    printk("\n");
    printk("========================================\n");
    printk(" HDLC-like Framing Demo (RFC 1662)\n");
    printk(" Raspberry Pi <-> Laptop Communication\n");
    printk("========================================\n\n");
    
    uint8_t tx_payload[256];
    uint8_t rx_payload[256];
    
    // Example: Send a test packet
    const char *test_msg = "Hello from Raspberry Pi!";
    unsigned msg_len = strlen(test_msg);
    
    printk("Sending test message: '%s'\n", test_msg);
    
    // Copy message to payload buffer
    for (unsigned i = 0; i < msg_len; i++) {
        tx_payload[i] = test_msg[i];
    }
    
    // Send framed packet
    int result = hdlc_send(tx_payload, msg_len);
    if (result == HDLC_OK) {
        printk("Packet sent successfully\n");
    } else {
        printk("Error sending packet: %d\n", result);
    }
    
    printk("\nWaiting to receive framed packets...\n");
    printk("(Send framed data from laptop to test reception)\n\n");
    
    // Receive loop
    while (1) {
        int rx_len = hdlc_recv(rx_payload, sizeof(rx_payload));
        
        if (rx_len > 0) {
            printk("Received %d bytes: ", rx_len);
            for (int i = 0; i < rx_len; i++) {
                if (rx_payload[i] >= 32 && rx_payload[i] < 127) {
                    printk("%c", rx_payload[i]);
                } else {
                    printk("\\x%02x", rx_payload[i]);
                }
            }
            printk("\n");
            
            // Echo back the received packet
            printk("Echoing back...\n");
            hdlc_send(rx_payload, rx_len);
        } else if (rx_len == HDLC_FCS_ERROR) {
            printk("FCS error: packet corrupted\n");
        } else if (rx_len == HDLC_TOO_LARGE) {
            printk("Packet too large\n");
        } else if (rx_len == HDLC_INCOMPLETE) {
            // This is normal when waiting for data
            continue;
        } else if (rx_len < 0) {
            printk("Receive error: %d\n", rx_len);
        }
    }
}
