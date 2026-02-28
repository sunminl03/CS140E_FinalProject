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
    gpio_lev0  = (GPIO_BASE + 0x34),
    gpio_pud = (GPIO_BASE + 0x94),
    gpio_pudclk0 = (GPIO_BASE + 0x98),
    gpio_pudclk1 = (GPIO_BASE + 0x9C)

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
    gpio_set_function(pin, GPIO_FUNC_OUTPUT);
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
    unsigned address2 = 0x20200020; // GPSET1
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
    gpio_set_function(pin, GPIO_FUNC_INPUT);
}

// Return 1 if <pin> is on, 0 if not.
int gpio_read(unsigned pin) {
    unsigned v = 0;

    if(pin > GPIO_MAX_PIN)
        gpio_panic("illegal pin=%d\n", pin);

    // unsigned address1 = 0x20200034; // GPLEV0
    // unsigned address2 = 0x20200038; // GPLEV1
    unsigned value = 0x0u;
    if (pin < 32){
        value = GET32(gpio_lev0);
    } else {
        value = GET32(gpio_lev0+4);
    }
    return (value & (1u << (pin % 32))) ? 1 : 0;
}

// set GPIO function for <pin> (input, output, alt...).
// settings for other pins should be unchanged.
void gpio_set_function(unsigned pin, gpio_func_t func) {
    if(pin > GPIO_MAX_PIN)
        gpio_panic("illegal pin=%d\n", pin);
    if((func & 0b111) != func)
        gpio_panic("illegal func=%x\n", func);
    // 10 pins per register
    unsigned modular_val = pin % 10;
    // only modify the three digits of that pin
    unsigned shift = 3 * modular_val;
    unsigned mask = 0x7u << shift;
    unsigned address = GPIO_BASE + pin/10 * 4;
    // unsigned address = 0x0u;
    // // unsigned address = GPIO_BASE + pin/10 * 4;
    // if (pin < 10){ //GPFSEL0: 0x2020 0000
    //     address = 0x20200000;
    // } else if (pin < 20){ //GPFSEL1: 0x2020 0004
    //     address = 0x20200004;
    // } else if (pin < 30){ //GPFSEL2: 0x2020 0008
    //     address = 0x20200008;
    // } else if (pin < 40){ //GPFSEL3: 0x2020 000C
    //     address = 0x2020000C;
    // } else if (pin < 50){ //GPFSEL4: 0x2020 0010
    //     address = 0x20200010;
    // } else if (pin < 60){ //GPFSEL5: 0x2020 0014
    //     address = 0x20200014;
    // }
    unsigned value = GET32(address);
    // clear out the three bits of that pin 
    value &= ~mask;
    // unsigned bits = 0x0u;
    // if (func == GPIO_FUNC_OUTPUT) {
    //     bits = 0x1u;
    // }
    // // set that pin to be the output 
    value |= func << shift;
    PUT32(address, value);
}

void gpio_set_pullup(unsigned pin) {
    // set GPPUD to 10 for pullup 
    PUT32(gpio_pud, 2);
    // setup time
    delay_us(150);
    // Pull up that pin
    if (pin < 32) {
        PUT32(gpio_pudclk0, (1 << pin));
    } else {
        PUT32(gpio_pudclk1, (1 << (pin % 32)));
    }
    
    // hold time 
    delay_us(150);
    // delete signal 
    PUT32(gpio_pud, 0x00);
    // delete clock 
    if (pin < 32) {
        PUT32(gpio_pudclk0, 0x00);
    } else {
        PUT32(gpio_pudclk1, 0x00);
    }

}

void gpio_set_pulldown(unsigned pin) {

        // set GPPUD to 10 for pullup 
    PUT32(gpio_pud, 0x1);
    // setup time
    delay_us(150);
    // Pull down that pin
    if (pin < 32) {
        PUT32(gpio_pudclk0, (1 << pin));
    } else {
        PUT32(gpio_pudclk1, (1 << (pin % 32)));
    }
    
    // hold time 
    delay_us(150);
    // delete signal 
    PUT32(gpio_pud, 0x00);
    // delete clock 
    if (pin < 32) {
        PUT32(gpio_pudclk0, 0x00);
    } else {
        PUT32(gpio_pudclk1, 0x00);
    }
}