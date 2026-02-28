// engler, cs140 put your gpio-int implementations in here.
#include "rpi.h"

// in libpi/include: has useful enums.
#include "rpi-interrupts.h"

enum {
    // Max gpio pin number.
    GPIO_MAX_PIN = 53,

    GPIO_BASE = 0x20200000,
    gpio_set0  = (GPIO_BASE + 0x1C),
    gpio_clr0  = (GPIO_BASE + 0x28),
    gpio_lev0  = (GPIO_BASE + 0x34), 
    GPREN_0 = 0x2020004C,
    GPREN_1 = 0x20200050,
    GPFEN_0 = 0x20200058, 
    GPFEN_1 = 0x2020005C, 
    GPEDS_0 = 0x20200040, 
    GPEDS_1 = 0x20200044, 
    IRQ_PENDING2 = 0x2000B208,
    IRQ_ENABLE2 = 0x2000B214

    // <you will need other values from BCM2835!>
};

// returns 1 if there is currently a GPIO_INT0 interrupt, 
// 0 otherwise.
//
// note: we can only get interrupts for <GPIO_INT0> since the
// (the other pins are inaccessible for external devices).
int gpio_has_interrupt(void) {
    // todo("implement: is there a GPIO_INT0 interrupt?\n");
    dev_barrier();
    uint32_t value = GET32(IRQ_PENDING2);
    dev_barrier();
    uint32_t mask = 0b1 << 17;
    return ((mask & value) > 0);
    
}

// p97 set to detect rising edge (0->1) on <pin>.
// as the broadcom doc states, it  detects by sampling based on the clock.
// it looks for "011" (low, hi, hi) to suppress noise.  i.e., its triggered only
// *after* a 1 reading has been sampled twice, so there will be delay.
// if you want lower latency, you should us async rising edge (p99)
//
// also have to enable GPIO interrupts at all in <IRQ_Enable_2>
void gpio_int_rising_edge(unsigned pin) {
    dev_barrier();
    if(pin>=32)
        return;
    // unsigned address = pin < 32? GPFEN_0 : GPFEN_1;
    unsigned mask = 1u << (pin % 32);
    OR32(GPREN_0, mask);
    dev_barrier();
    PUT32(IRQ_ENABLE2,1u << 17);
    dev_barrier();

}

// p98: detect falling edge (1->0).  sampled using the system clock.  
// similarly to rising edge detection, it suppresses noise by looking for
// "100" --- i.e., is triggered after two readings of "0" and so the 
// interrupt is delayed two clock cycles.   if you want  lower latency,
// you should use async falling edge. (p99)
//
// also have to enable GPIO interrupts at all in <IRQ_Enable_2>
void gpio_int_falling_edge(unsigned pin) {
    dev_barrier();
    if(pin>=32)
        return;
    // unsigned address = pin < 32? GPFEN_0 : GPFEN_1;
    unsigned mask = 1u << (pin % 32);
    OR32(GPFEN_0, mask);
    dev_barrier();
    PUT32(IRQ_ENABLE2,1u << 17);
    dev_barrier();
}

// p96: a 1<<pin is set in EVENT_DETECT if <pin> triggered an interrupt.
// if you configure multiple events to lead to interrupts, you will have to 
// read the pin to determine which caused it.
int gpio_event_detected(unsigned pin) {
    if(pin>=32)
        return 0;
    dev_barrier();
    unsigned address = pin < 32? GPEDS_0 : GPEDS_1;
    unsigned mask = 1u << (pin % 32);
    unsigned value = GET32(address); 
    dev_barrier();
    if ((mask & value) != 0) { // check if pin is set to 1 (read)
        return 1;
    }
    return 0;
}

// p96: have to write a 1 to the pin to clear the event.
void gpio_event_clear(unsigned pin) {
    if(pin>=32)
        return;
    dev_barrier();
    unsigned address = pin < 32? GPEDS_0 : GPEDS_1;
    unsigned value = 1u << (pin % 32); // need to write a 1 to clear it (write). not read modify write. 
    PUT32(address, value);
    dev_barrier();
}
