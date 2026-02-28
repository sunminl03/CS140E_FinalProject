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
    GPREN_0 = 0x2020004C,
    GPREN_1 = 0x20200050,
    GPFEN_0 = 0x20200054, 
    GPFEN_1 = 0x20200058, 
    GPEDS_0 = 0x20200040, 
    GPEDS_1 = 0x20200044

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
// static void or32(volatile void *addr, uint32_t val) {
//     device_barrier();
//     put32(addr, get32(addr) | val);
//     device_barrier();
// }
// static void OR32(uint32_t addr, uint32_t val) {
//     or32((volatile void*)addr, val);
// }

// void gpio_int_rising_edge(unsigned pin) {
//     unsigned address = pin < 32? GPREN_0 : GPREN_1;
//     unsigned mask = 1u << (pin % 32);
//     OR32(address, mask);
// }

// void gpio_int_falling_edge(unsigned pin) {
//     unsigned address = pin < 32? GPFEN_0 : GPFEN_1;
//     unsigned mask = 1u << (pin % 32);
//     OR32(address, mask);

// }

// int gpio_event_detected(unsigned pin) {
//     dev_barrier();
//     unsigned address = pin < 32? GPEDS_0 : GPEDS_1;
//     unsigned mask = 1u << (pin % 32);
//     unsigned value = GET32(address); 
//     dev_barrier();
//     if ((mask & value) != 0) { // check if pin is set to 1 (read)
//         return 1;
//     }
//     return 0;
// }

// void gpio_event_clear(unsigned pin) {
//     dev_barrier();
//     unsigned address = pin < 32? GPEDS_0 : GPEDS_1;
//     unsigned value = 1u << (pin % 32); // need to write a 1 to clear it (write). not read modify write. 
//     PUT32(address, value);
//     dev_barrier();
// }

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
// #include "rpi.h"

// // See broadcomm documents for magic addresses and magic values.
// //
// // If you pass addresses as:
// //  - pointers use put32/get32.
// //  - integers: use PUT32/GET32.
// //  semantics are the same.
// enum {
//     // Max gpio pin number.
//     GPIO_MAX_PIN = 53,
//     GPIO_MAX_FN_VALUE = 7,

//     GPIO_BASE = 0x20200000,
//     gpio_set0  = (GPIO_BASE + 0x1C),
//     gpio_clr0  = (GPIO_BASE + 0x28),
//     gpio_lev0  = (GPIO_BASE + 0x34)

//     // <you will need other values from BCM2835!>
// };

// //
// // Part 1 implement gpio_set_on, gpio_set_off, gpio_set_output
// //

// // set <pin> to be an output pin.
// //
// // NOTE: fsel0, fsel1, fsel2 are contiguous in memory, so you
// // can (and should) use ptr calculations versus if-statements!
// void gpio_set_output(unsigned pin) {
//     if(pin > GPIO_MAX_PIN)
//         gpio_panic("illegal pin=%d\n", pin);
//     gpio_set_function(pin, GPIO_FUNC_OUTPUT);
// }

// // Set GPIO <pin> = on.
// void gpio_set_on(unsigned pin) {
//     if(pin > GPIO_MAX_PIN)
//         gpio_panic("illegal pin=%d\n", pin);

//     // Implement this. 
//     // NOTE: 
//     //  - If you want to be slick, you can exploit the fact that 
//     //    SET0/SET1 are contiguous in memory.
//     unsigned *addr = (unsigned *)gpio_set0 + pin / 32; // gpset addresses: pg 90
//     unsigned value = 0b1 << pin % 32;
//     put32(addr, value);
// }

// // Set GPIO <pin> = off
// void gpio_set_off(unsigned pin) {
//     if(pin > GPIO_MAX_PIN)
//         gpio_panic("illegal pin=%d\n", pin); 
//     // Implement this. 
//     // NOTE: 
//     //  - If you want to be slick, you can exploit the fact that 
//     //    CLR0/CLR1 are contiguous in memory.
//     unsigned *addr = (unsigned *)gpio_clr0 + pin / 32; // gpclr addresses: pg 90
//     unsigned value = 0b1 << pin % 32;
//     put32(addr, value);
// }

// // Set <pin> to <v> (v \in {0,1})
// void gpio_write(unsigned pin, unsigned v) {
//     if(v)
//         gpio_set_on(pin);
//     else
//         gpio_set_off(pin);
// }

// //
// // Part 2: implement gpio_set_input and gpio_read
// //

// // set <pin> = input.
// void gpio_set_input(unsigned pin) {
//     if(pin > GPIO_MAX_PIN)
//         gpio_panic("illegal pin=%d\n", pin);
//     gpio_set_function(pin, GPIO_FUNC_INPUT);
// }

// // Return 1 if <pin> is on, 0 if not.
// int gpio_read(unsigned pin) {
//     unsigned v = 0;

//     if(pin > GPIO_MAX_PIN)
//         gpio_panic("illegal pin=%d\n", pin);

//     unsigned *addr = (unsigned *)gpio_lev0 + pin / 32; // gpset addresses: pg 90
//     v = get32(addr);
//     v >>= pin % 32;
//     return v & 0b1;
// }

// // set GPIO function for <pin> (input, output, alt...).
// // settings for other pins should be unchanged.
// void gpio_set_function(unsigned pin, gpio_func_t function) {
//     if(pin > GPIO_MAX_PIN)
//         gpio_panic("illegal pin=%d\n", pin);
//     if((function & 0b111) != function)
//         gpio_panic("illegal func=%x\n", function);
//     unsigned *addr = (unsigned *)GPIO_BASE + pin / 10; // fsel addresses: pg 90
//     unsigned value = get32(addr);
//     unsigned shift = pin % 10 * 3; 
//     unsigned mask = 0b111 << shift; // set a mask that when negated, will clear the required bits
//     value &= ~mask;
//     value |= function << shift;
//     put32(addr, value);
// }