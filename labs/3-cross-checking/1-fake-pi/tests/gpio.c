/*
 * Implement the following routines to set GPIO pins to input or 
 * output, and to read (input) and write (output) them.
 *  1. DO NOT USE loads and stores directly: only use GET32 and 
 *    PUT32 to read and write memory.  See <start.S> for thier
 *    definitions.
 *  2. DO USE the minimum number of such calls.
 * (Both of these matter for the next lab.)
 *
 * See <rpi.h> in this directory for the definitions.
 *  - we use <gpio_panic> to try to catch errors.  For lab 2
 *    it only infinite loops since we don't have <printk>
 */
#include "rpi.h"

// See broadcomm documents for magic addresses and magic values.
//
// If you pass addresses as:
//  - pointers use put32/get32.
//  - integers: use PUT32/GET32.
//  semantics are the same.
enum {
    // Max gpio pin number.
    GPIO_MAX_PIN = 53,

    GPIO_BASE = 0x20200000,
    gpio_set0  = (GPIO_BASE + 0x1C),
    gpio_clr0  = (GPIO_BASE + 0x28),
    gpio_lev0  = (GPIO_BASE + 0x34)

    // <you will need other values from BCM2835!>
};

//
// Part 1 implement gpio_set_on, gpio_set_off, gpio_set_output
//

// set <pin> to be an output pin.
//
// NOTE: fsel0, fsel1, fsel2 are contiguous in memory, so you
// can (and should) use ptr calculations versus if-statements!
void gpio_set_output(unsigned pin) {
    if(pin > GPIO_MAX_PIN)
        gpio_panic("illegal pin=%d\n", pin);
    // 10 pins per register
    unsigned modular_val = pin % 10;
    // only modify the three digits of that pin
    unsigned shift = 3 * modular_val;
    unsigned mask = 0x7u << shift;
    unsigned address = 0x0u;
    if (pin < 10){ //GPFSEL0: 0x2020 0000
        address = 0x20200000;
    } else if (pin < 20){ //GPFSEL1: 0x2020 0004
        address = 0x20200004;
    } else if (pin < 30){ //GPFSEL2: 0x2020 0008
        address = 0x20200008;
    }
    // read-modify-write
    unsigned value = GET32(address);
    // clear out the three bits of that pin 
    value &= ~mask;
    unsigned bits = 0x00000001;
    // set that pin to be the output 
    value |= bits << shift;
    PUT32(address, value);
}

void gpio_set_txpin(unsigned pin) {
    if(pin > GPIO_MAX_PIN)
        gpio_panic("illegal pin=%d\n", pin);
    // 10 pins per register
    unsigned modular_val = pin % 10;
    // only modify the three digits of that pin
    unsigned shift = 3 * modular_val;
    unsigned mask = 0x7u << shift;
    unsigned address = 0x20200004;
    
    // read-modify-write
    unsigned value = GET32(address);
    // clear out the three bits of that pin 
    value &= ~mask;
    unsigned bits = 0x00000004;
    // set that pin to be the output 
    value |= bits << shift;
    PUT32(address, value);
}

// Set GPIO <pin> = on.
void gpio_set_on(unsigned pin) {
    if(pin > GPIO_MAX_PIN)
        gpio_panic("illegal pin=%d\n", pin);

    // Implement this. 
    // NOTE: 
    //  - If you want to be slick, you can exploit the fact that 
    //    SET0/SET1 are contiguous in memory.
    unsigned address1 = 0x2020001C; // GPSET0
    unsigned address2 = 0x20200020; // GPFSET1
    unsigned bits = 0x1u << (pin % 32);
    if (pin < 32){
        PUT32(address1, bits);
    } else {
        PUT32(address2, bits);
    }


}

// Set GPIO <pin> = off
void gpio_set_off(unsigned pin) {
    if(pin > GPIO_MAX_PIN)
        gpio_panic("illegal pin=%d\n", pin);

    // Implement this. 
    // NOTE: 
    //  - If you want to be slick, you can exploit the fact that 
    //    CLR0/CLR1 are contiguous in memory.
    unsigned address1 = 0x20200028; // GPCLR0
    unsigned address2 = 0x2020002C; // GPCLR1
    unsigned bits = 0x1u << (pin % 32);
    if (pin < 32){
        PUT32(address1, bits);
    } else {
        PUT32(address2, bits);
    }
}

// Set <pin> to <v> (v \in {0,1})
void gpio_write(unsigned pin, unsigned v) {
    if(v)
        gpio_set_on(pin);
    else
        gpio_set_off(pin);
}

//
// Part 2: implement gpio_set_input and gpio_read
//

// set <pin> = input.
void gpio_set_input(unsigned pin) {
    if(pin > GPIO_MAX_PIN)
        gpio_panic("illegal pin=%d\n", pin);

    if(pin > GPIO_MAX_PIN)
        gpio_panic("illegal pin=%d\n", pin);
    // 10 pins per register
    unsigned modular_val = pin % 10;
    // only modify the three digits of that pin
    unsigned shift = 3 * modular_val;
    unsigned mask = 0x7u << shift;
    unsigned address = 0x0u;
    if (pin < 10){ //GPFSEL0: 0x2020 0000
        address = 0x20200000;
    } else if (pin < 20){ //GPFSEL1: 0x2020 0004
        address = 0x20200004;
    } else if (pin < 30){ //GPFSEL2: 0x2020 0008
        address = 0x20200008;
    }
    // read-modify-write
    unsigned value = GET32(address);
    // clear out the three bits of that pin 
    value &= ~mask;
    // unsigned bits = 0x00000000;
    // // set that pin to be the output 
    // value |= bits << shift;
    PUT32(address, value);
}

// Return 1 if <pin> is on, 0 if not.
int gpio_read(unsigned pin) {
    unsigned v = 0;

    if(pin > GPIO_MAX_PIN)
        gpio_panic("illegal pin=%d\n", pin);

    unsigned address1 = 0x20200034; // GPLEV0
    unsigned address2 = 0x20200038; // GPLEV1
    unsigned value = 0x0u;
    if (pin < 32){
        value = GET32(address1);
    } else {
        value = GET32(address2);
    }
    unsigned mod_pin = pin % 32; 
    unsigned mask = 0x1u << mod_pin;
    value &= mask;
    v = value;


    return v;
}
