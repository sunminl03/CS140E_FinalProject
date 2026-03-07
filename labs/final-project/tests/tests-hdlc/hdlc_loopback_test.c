// pi code for the hdlc loopback test
#include "rpi.h"
#include "hdlc_framing.h"

/*
 * How this test works:
 * 1. Wait until a frame is received.
 * 2. Print the received frame.
 * 3. Send the same frame back to the laptop.
 *
 * Need to kill make in between:
 * When I run the pi code, my-install holds the serial port open and keeps
 * reading due to pi_echo(). Python script tries to open the port, but it
 * can't because my-install is holding it open.
 *
 * It's okay to kill make because:
 *   - The program is already loaded and running on the Pi
 *   - The Pi uses its own UART hardware, not the laptop's connection
 *   - pi_echo() is only for monitoring — not required for the program
 *   - Killing my-install only closes the laptop's connection, not the Pi's UART
 */

static void dump_payload(const char *msg, const uint8_t *buf, unsigned n) {
    // Prints a label and byte count
    printk("%s (%u bytes): ", msg, n);
    for (unsigned i = 0; i < n; i++) {
        uint8_t b = buf[i];
        // printk doesn't support %02x, so pad manually
        if (b < 0x10) printk("0");
        // Prints each byte in hex
        printk("%x ", b);
    }
    printk("\n");
}

void notmain(void) {
    uart_init();

    printk("HDLC loopback test starting...\n");

    while (1) {
        uint8_t payload[256];
        int n = hdlc_recv(payload, sizeof(payload));

        if (n < 0) {
            printk("hdlc_recv error=%d\n", n);
            continue;
        }
        // at this point, the payload has been received and is in the payload array
        dump_payload("received", payload, n);

        // Echo same payload back to laptop using hdlc_send().
        int ret = hdlc_send(payload, n);
        if (ret < 0) {
            printk("hdlc_send error=%d\n", ret);
        } else {
            // Small delay to ensure HDLC frame is fully transmitted before printk
            delay_ms(10);
            printk("echoed %d bytes back\n", n);
        }
    }
}