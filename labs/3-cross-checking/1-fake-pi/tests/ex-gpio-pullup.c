#include "rpi.h"

void notmain(void) {
    gpio_set_input(20);
    gpio_set_pullup(20);

    gpio_set_input(27);
    gpio_set_pulldown(27);
}