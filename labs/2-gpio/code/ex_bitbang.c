#include "rpi.h"

static inline void cycle_cnt_init(void) {
    uint32_t in = 1;
    asm volatile("MCR p15, 0, %0, c15, c12, 0" :: "r"(in));
}

// read cycle counter
static inline uint32_t cycle_cnt_read(void) {
    uint32_t out;
    asm volatile("MRC p15, 0, %0, c15, c12, 1" : "=r"(out));
    return out;
}

static inline void delay_cc(uint32_t cycles) {
    uint32_t start = cycle_cnt_read();
    while ((uint32_t)(cycle_cnt_read() - start) < cycles)
        ;
}

void uart_transmit(unsigned char x) {
    unsigned tx_pin = 14;
    const unsigned BIT_CYCLES = 5500;
    // Transmit
    gpio_write(tx_pin, 0);
    delay_cc(BIT_CYCLES);
    for (int i = 0; i < 8; i++){
        gpio_write(tx_pin, (x >> i) & 1);
        delay_cc(BIT_CYCLES);
    }
    gpio_write(tx_pin, 1);
    delay_cc(BIT_CYCLES);
    delay_cc(BIT_CYCLES);

}
void notmain(void) {
    // Initialize
    const unsigned BIT_CYCLES = 5500;
    unsigned tx_pin = 14;

    gpio_set_output(tx_pin);
    gpio_write(tx_pin, 1);

    cycle_cnt_init();
    while (1) {
        uart_transmit('h');
        uart_transmit('e');
        uart_transmit('l');
        uart_transmit('l');
        uart_transmit('o');
        uart_transmit(' ');
        uart_transmit('w');
        uart_transmit('o');
        uart_transmit('r');
        uart_transmit('l');
        uart_transmit('d');
        uart_transmit('\r');
        uart_transmit('\n');
        delay_cc(BIT_CYCLES);
    }

}