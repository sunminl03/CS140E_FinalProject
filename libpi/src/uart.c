// simple mini-uart driver: implement every routine 
// with a <todo>.
//
// NOTE: 
//  - from broadcom: if you are writing to different 
//    devices you MUST use a dev_barrier().   
//  - its not always clear when X and Y are different
//    devices.
//  - pay attenton for errata!   there are some serious
//    ones here.  if you have a week free you'd learn 
//    alot figuring out what these are (esp hard given
//    the lack of printing) but you'd learn alot, and
//    definitely have new-found respect to the pioneers
//    that worked out the bcm eratta.
//
// historically a problem with writing UART code for
// this class (and for human history) is that when 
// things go wrong you can't print since doing so uses
// uart.  thus, debugging is very old school circa
// 1950s, which modern brains arne't built for out of
// the box.   you have two options:
//  1. think hard.  we recommend this.
//  2. use the included bit-banging sw uart routine
//     to print.   this makes things much easier.
//     but if you do make sure you delete it at the 
//     end, otherwise your GPIO will be in a bad state.
//
// in either case, in the next part of the lab you'll
// implement bit-banged UART yourself.
#include "rpi.h"

// change "1" to "0" if you want to comment out
// the entire block.
#if 1
//*****************************************************
// We provide a bit-banged version of UART for debugging
// your UART code.  delete when done!
//
// NOTE: if you call <emergency_printk>, it takes 
// over the UART GPIO pins (14,15). Thus, your UART 
// GPIO initialization will get destroyed.  Do not 
// forget!   

// header in <libpi/include/sw-uart.h>
#include "sw-uart.h"
static sw_uart_t sw_uart;

// a sw-uart putc implementation.
static int sw_uart_putc(int chr) {
    sw_uart_put8(&sw_uart,chr);
    return chr;
}

// call this routine to print stuff. 
//
// note the function pointer hack: after you call it 
// once can call the regular printk etc.
__attribute__((noreturn)) 
static void emergency_printk(const char *fmt, ...)  {
    // we forcibly initialize in case the 
    // GPIO got reset. this will setup 
    // gpio 14,15 for sw-uart.
    sw_uart = sw_uart_default();

    // all libpi output is via a <putc>
    // function pointer: this installs ours
    // instead of the default
    rpi_putchar_set(sw_uart_putc);

    // do print
    va_list args;
    va_start(args, fmt);
    vprintk(fmt, args);
    va_end(args);

    // at this point UART is all messed up b/c we took it over
    // so just reboot.   we've set the putchar so this will work
    clean_reboot();
}

#undef todo
#define todo(msg) do {                          \
    emergency_printk("%s:%d:%s\nDONE!!!\n",     \
            __FUNCTION__,__LINE__,msg);         \
} while(0)

// END of the bit bang code.
#endif


//*****************************************************
// the rest you should implement.

// called first to setup uart to 8n1 115200  baud,
// no interrupts.
//  - you will need memory barriers, use <dev_barrier()>
//
//  later: should add an init that takes a baud rate.

enum {
    AUX_IRQ = 0x20215000,
    AUX_ENABLES = 0x20215004,
    
    AUX_MU_IO_REG = 0x20215040,
    AUX_MU_IER_REG = AUX_MU_IO_REG + 4,
    AUX_MU_IIR_REG = AUX_MU_IO_REG + 8,
    AUX_MU_LCR_REG = AUX_MU_IO_REG + 12,
    AUX_MU_MCR_REG = AUX_MU_IO_REG + 16,
    AUX_MU_LSR_REG = AUX_MU_IO_REG + 20,
    AUX_MU_MSR_REG = AUX_MU_IO_REG + 24,
    AUX_MU_SCRATCH = AUX_MU_IO_REG + 28,
    AUX_MU_CNTL_REG = AUX_MU_IO_REG + 32,
    AUX_MU_STAT_REG = AUX_MU_IO_REG + 36,
    AUX_MU_BAUD_REG = AUX_MU_IO_REG + 40
};
void uart_init(void) {
    // todo("start here\n");

    // NOTE: for cross-checking: make sure write UART 
    // addresses in order
    // // 8. Set GPIO pins
    dev_barrier();
    gpio_set_function(14, GPIO_FUNC_ALT5);
    gpio_set_function(15, GPIO_FUNC_ALT5);
    dev_barrier();

    // 1. Turn on UART
    unsigned value = GET32(AUX_ENABLES);
    unsigned mask = 0x00000001;
    value |= mask;
    PUT32(AUX_ENABLES, value);
    
    dev_barrier();
    // 2. Disable TX/RX
    PUT32(AUX_MU_CNTL_REG, 0x00000000);
    // 3. Set regs we can ignore to default values
    PUT32(AUX_MU_MCR_REG, 0x00000000);
    // PUT32(AUX_MU_MSR_REG, 0x00000000);
    // PUT32(AUX_MU_SCRATCH, 0x00000000);
    // 4. Find and clear all parts of the UART state
    // 4-1. clear both fifos 
    // PUT32(AUX_MU_IIR_REG, 0x000000C3); 
    PUT32(AUX_MU_IIR_REG, 0x6); 
    // 5. Disable Interrupts
    // value = GET32(AUX_MU_IER_REG);
    // mask = 0xFFFFFFFC;
    // value &= mask;
    PUT32(AUX_MU_IER_REG, 0x0);
    // 6. Set Baudrate to 115200
    value = 0x0000010E; // 270
    PUT32(AUX_MU_BAUD_REG, value);
    // 7. 8 bits one stop bit
    // value = GET32(AUX_MU_LCR_REG);
    // mask = 0b11;
    // value |= mask;
    PUT32(AUX_MU_LCR_REG, 0x3);

    // // // 8. Set GPIO pins
    // dev_barrier();
    // gpio_set_function(14, GPIO_FUNC_ALT5);
    // gpio_set_function(15, GPIO_FUNC_ALT5);
    // dev_barrier();

    // 9. Enable TX/RX
    PUT32(AUX_MU_CNTL_REG, 0b11);
    dev_barrier();



}

// disable the uart: make sure all bytes have been 
// 
void uart_disable(void) {
    dev_barrier();
    uart_flush_tx();
    PUT32(AUX_MU_CNTL_REG, 0x0);
    uint32_t mask = 0xFFFFFFFE; 
    dev_barrier();
    uint32_t value = GET32(AUX_ENABLES);
    PUT32(AUX_ENABLES, value & mask);
    dev_barrier();

}

// returns one byte from the RX (input) hardware
// FIFO.  if FIFO is empty, blocks until there is 
// at least one byte.
int uart_get8(void) {
    dev_barrier();
    while (!uart_has_data()) {

    }
    unsigned mask = 0xFF;
    uint32_t value = mask & GET32(AUX_MU_IO_REG);
    dev_barrier();
    return value;

}

// returns 1 if the hardware TX (output) FIFO has room
// for at least one byte.  returns 0 otherwise.
int uart_can_put8(void) {
    dev_barrier();
    uint32_t val = (GET32(AUX_MU_LSR_REG)&0b100000) > 0;
    dev_barrier();
    return val;
    
}

// put one byte on the TX FIFO, if necessary, waits
// until the FIFO has space.
int uart_put8(uint8_t c) {
    // uint32_t value = uint32_t(c);
    dev_barrier();
    while (!uart_can_put8()) {
    }
    PUT32(AUX_MU_IO_REG, c);
    dev_barrier();
    return 1;
    
}

// returns:
//  - 1 if at least one byte on the hardware RX FIFO.
//  - 0 otherwise
int uart_has_data(void) {
    dev_barrier();
    uint32_t val = (GET32(AUX_MU_LSR_REG) & 0b1) > 0;
    dev_barrier();
    return val;
    
    
}

// returns:
//  -1 if no data on the RX FIFO.
//  otherwise reads a byte and returns it.
int uart_get8_async(void) { 

    if(!uart_has_data()) 
        return -1;

    return uart_get8();
    
}

// returns:
//  - 1 if TX FIFO empty AND idle.
//  - 0 if not empty.
int uart_tx_is_empty(void) {
    dev_barrier();
    uint32_t value = (GET32(AUX_MU_LSR_REG)& 0b1000000) > 0;
    dev_barrier();
    return value;
    
}

// return only when the TX FIFO is empty AND the
// TX transmitter is idle.  
//
// used when rebooting or turning off the UART to
// make sure that any output has been completely 
// transmitted.  otherwise can get truncated 
// if reboot happens before all bytes have been
// received.
void uart_flush_tx(void) {
   
    while(!uart_tx_is_empty())
        rpi_wait();

}
