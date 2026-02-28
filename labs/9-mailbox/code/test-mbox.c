#include "rpi.h"
#include "mbox.h"

uint32_t rpi_temp_get(void) ;
#include "pl011-uart.h"

#include "cycle-count.h"

// compute cycles per second using
//  - cycle_cnt_read();
//  - timer_get_usec();
unsigned cyc_per_sec(void) {
    uint32_t c = cycle_cnt_read();
    uint32_t s = timer_get_usec();
    while (timer_get_usec() - s < 1000000) {
    }
    return cycle_cnt_read() - c;
}

uint32_t measure_put32(volatile uint32_t *ptr) ;
uint32_t measure_get32(volatile uint32_t *ptr) ;
uint32_t measure_ptr_write(volatile uint32_t *ptr);
uint32_t measure_ptr_read(volatile uint32_t *ptr);
    
void measure(const char *msg) {

    printk("------------------------------------------------------       \n");
    printk("measuring: <%s>\n", msg);
    
    uint32_t x;

    // Q: try switching --- does this pattern make a difference for
    //    any measurement?
#if 0
    let t0 = measure_put32(&x);
    let t1 = measure_put32(&x);
    printk("\tcall to PUT32  =\t%d cycles\n", t0);
    printk("\tcall to PUT32  =\t%d cycles\n", t1);
#else
    uint32_t c_per_s = cyc_per_sec();
    uint32_t c_per_us = c_per_s /1000000; // cycles per us
    uint32_t c = 0;
    for (int i = 0; i < 10000; i++) {
        c += measure_put32(&x);
    }
    printk("\tPUT32 for 10000 cycles      =\t %d us\n", (c/c_per_us));
    // printk("\tcall to PUT32  =\t%d cycles, %d ns\n", c, (int)((c/c_per_8s)*10));
#endif

    enum { BASE = 0x20200000 };
    // periph write to try with a set0 (safe)
    let set0 = (void*)(BASE + 0x1C);
    // periph read from level (safe)
    let level0 = (void*)(BASE + 0x34);

    // Q: why not much difference if you double?   what if you
    // defer the printk?

    c = 0;
    for (int i = 0; i < 10000; i++) {
        c += measure_get32(&x);
    }
    printk("\tGET32 for 10000 cycles      =\t %d us\n", (c/c_per_us));
    
    c = 0;
    for (int i = 0; i < 10000; i++) {
        c += measure_ptr_write(&x);
    }
    printk("\tptr write for 10000 cycles      =\t %d us\n", (c/c_per_us));

    for (int i = 0; i < 10000; i++) {
        c += measure_ptr_read(&x);
    }
    printk("\tptr read  for 10000 cycles      =\t %d us\n", (c/c_per_us));

    for (int i = 0; i < 10000; i++) {
        c += measure_ptr_write(set0);
    }
    printk("\tperiph write  for 10000 cycles      =\t %d us\n", (c/c_per_us));

    for (int i = 0; i < 10000; i++) {
        c += measure_ptr_read(level0);
    }
    printk("\tperiph read  for 10000 cycles      =\t %d us\n", (c/c_per_us));

    printk("                ----------------------------       \n");
    asm volatile(".align 5");
    let s = cycle_cnt_read();
    *(volatile uint32_t *)0 = 4;
    let e = cycle_cnt_read();
    printk("\taligned null pointer write =%d cycles\n", e-s);

    // use macro <TIME_CYC_PRINT> to clean up a bit.
    //  see: libpi/include/cycle-count.h
    TIME_CYC_PRINT("\tread/write barrier", dev_barrier());
    TIME_CYC_PRINT("\tread barrier      ", dmb());
    TIME_CYC_PRINT("\tsafe timer        ", timer_get_usec());
    TIME_CYC_PRINT("\tunsafe timer      ", timer_get_usec_raw());
    // macro expansion messes up the printk
    printk("\t<cycle_cnt_read()>: %d\n", TIME_CYC(cycle_cnt_read()));
    printk("------------------------------------------------------       \n");
}

// pull these out so we can see the machine code
uint32_t measure_put32(volatile uint32_t *ptr) {
    asm volatile(".align 5");
    let s = cycle_cnt_read();
        put32(ptr,0);
    let e = cycle_cnt_read();
    return e-s;
}
uint32_t measure_get32(volatile uint32_t *ptr) {
    asm volatile(".align 5");
    // simple example to measure GET32
    let s = cycle_cnt_read();
        get32(ptr);
    let e = cycle_cnt_read();
    return e-s;
}

uint32_t measure_ptr_write(volatile uint32_t *ptr) {
    asm volatile(".align 5");
    // a bit weird :)
    let s = cycle_cnt_read();
        *ptr = 4;
    let e = cycle_cnt_read();
    return e-s;
}

uint32_t measure_ptr_read(volatile uint32_t *ptr) {
    asm volatile(".align 5");
    let s = cycle_cnt_read();
        uint32_t x = *ptr;
    let e = cycle_cnt_read();
    return e-s;
}



void notmain(void) { 
    output("mailbox serial number = %llx\n", rpi_get_serialnum());
    output("mailbox model number = %x\n", rpi_get_model());
    output("mailbox revision number = %x\n", rpi_get_revision());
    uint32_t size = rpi_get_memsize();
    output("mailbox physical mem: size=%d (%dMB)\n", 
            size, 
            size/(1024*1024));

    // output("mailbox model number = %x\n", rpi_get_model());

    // uint32_t size = rpi_get_memsize();
    // output("mailbox physical mem: size=%d (%dMB)\n", 
    //         size, 
    //         size/(1024*1024));

    // print as fahrenheit
    unsigned x = rpi_temp_get();

    // convert <x> to C and F
    unsigned C = 0, F = 0;
    C = x / 1000;
    F = 9 * C / 5 + 32;
    output("mailbox temp = %x, C=%d F=%d\n", x, C, F); 

    // current clock tests
    unsigned cur_cpu_clock = rpi_clock_curhz_get(MBOX_CLK_CPU) /1000000;
    unsigned cur_core_clock = rpi_clock_curhz_get(MBOX_CLK_CORE) /1000000;
    unsigned cur_sdram_clock = rpi_clock_curhz_get(MBOX_CLK_SDRAM) /1000000;
    unsigned cur_v3d_clock = rpi_clock_curhz_get(MBOX_CLK_V3D) /1000000;
    output("current cpu clock rate = %d Mhz\n", cur_cpu_clock);
    output("current core clock rate = %d Mhz\n", cur_core_clock);
    output("current sdram clock rate = %d Mhz\n", cur_sdram_clock);
    output("current v3d clock rate = %d Mhz\n\n", cur_v3d_clock);

    // max clock paths
    unsigned max_cpu_clock = rpi_clock_maxhz_get(MBOX_CLK_CPU) /1000000;
    unsigned max_core_clock = rpi_clock_maxhz_get(MBOX_CLK_CORE) /1000000;
    unsigned max_sdram_clock = rpi_clock_maxhz_get(MBOX_CLK_SDRAM) /1000000;
    unsigned max_v3d_clock = rpi_clock_maxhz_get(MBOX_CLK_V3D) /1000000;
    output("max cpu clock rate = %d Mhz\n", max_cpu_clock);
    output("max core clock rate = %d Mhz\n", max_core_clock);
    output("max sdram clock rate = %d Mhz\n", max_sdram_clock);
    output("max v3d clock rate = %d Mhz\n\n", max_v3d_clock);
    unsigned cyc_start = cycle_cnt_read();

    unsigned s = timer_get_usec();
    while((timer_get_usec() - s) < 1000*1000)
        ;
    unsigned cyc_end = cycle_cnt_read();

    unsigned tot = cyc_end - cyc_start;
    printk("cycles per sec = %d\n", tot);

    unsigned exp = 700 * 1000 * 1000;
    printk("expected - measured = %d cycles of error\n", exp-tot);

    // set clock rate to max clock path
    rpi_clock_hz_set(MBOX_CLK_CPU, rpi_clock_maxhz_get(MBOX_CLK_CPU));
    rpi_clock_hz_set(MBOX_CLK_CORE, rpi_clock_maxhz_get(MBOX_CLK_CORE));
    rpi_clock_hz_set(MBOX_CLK_SDRAM, rpi_clock_maxhz_get(MBOX_CLK_SDRAM));
    rpi_clock_hz_set(MBOX_CLK_V3D, rpi_clock_maxhz_get(MBOX_CLK_V3D));

    // real clock paths
    pl011_console_init();
    unsigned real_cpu_clock = rpi_clock_realhz_get(MBOX_CLK_CPU) /1000000;
    unsigned real_core_clock = rpi_clock_realhz_get(MBOX_CLK_CORE) /1000000;
    unsigned real_sdram_clock = rpi_clock_realhz_get(MBOX_CLK_SDRAM) /1000000;
    unsigned real_v3d_clock = rpi_clock_realhz_get(MBOX_CLK_V3D) /1000000;
    output("real cpu clock rate = %d Mhz\n", real_cpu_clock);
    output("real core clock rate = %d Mhz\n", real_core_clock);
    output("real sdram clock rate = %d Mhz\n", real_sdram_clock);
    output("real v3d clock rate = %d Mhz\n\n", real_v3d_clock);

    printk("\nmeasuring cost of different operations (pi A+ = 700 cyc / usec)\n");
    cycle_cnt_init();

    measure("cache off");

    caches_enable();
    measure("with icache on first run");

    cyc_start = cycle_cnt_read();

    s = timer_get_usec();
    while((timer_get_usec() - s) < 1000*1000)
        ;
    cyc_end = cycle_cnt_read();

    tot = cyc_end - cyc_start;
    printk("cycles per sec = %d\n", tot);

    exp = 1150*1000 *1000;
    printk("expected - measured = %d cycles of error\n", exp-tot);
}
