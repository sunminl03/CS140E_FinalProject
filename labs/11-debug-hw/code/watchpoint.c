// very dumb, simple interface to wrap up watchpoints better.
// only handles a single watchpoint.
#include "rpi.h"
#include "watchpoint.h"
#include "asm-helpers.h"
#include "bit-support.h"
// DSCR pg. 13-7 ,
cp_asm_fn(cp14_dscr, p14, 0, c0, c1, 0) 

// WCR0 : enable or disable watchpoint
cp_asm_fn(cp14_wcr, p14, 0, c0, c0, 7)

// WVR0 : enable or disable watchpoint
cp_asm_fn(cp14_wvr, p14, 0, c0, c0, 6)

// DFSR : check cause of data fault 
cp_asm_fn(cp15_dfsr, p15, 0, c5, c0, 0)

// WFAR, pg 13-12, contains pc address of fault
cp_asm_fn(cp14_wfar, p14, 0, c0, c6, 0) 

// FAR: pg 13-68, contains data address of fault 
cp_asm_fn(cp15_far, p15, 0, c6, c0, 0)

// keep track of what we are watching.
static uint32_t watch_addr;

// was it a watchpoint fault?
//  1. use dfsr 3-64  to make sure it was a debug event.
//  2. and dscr 13-11: to make sure it was a watchpoint
int watchpt_fault_p(void) {
    uint32_t dfsr = cp15_dfsr_get();
    // check b0010 = Instruction debug event fault
    if ((dfsr & 2) == 0) {
        return 0;
    }
    uint32_t dscr = cp14_dscr_get();
    if ((dscr & 0b111100) == 0) {
        return 0;
    }
    return 1;

    // // 
    // return staff_watchpt_fault_p();
}

// is it a load fault?
//  - use dfsr 3-64
int watchpt_load_fault_p(void) {
    if(!watchpt_fault_p())
        return 0;
    // return staff_watchpt_load_fault_p();
    uint32_t dfsr = cp15_dfsr_get();
    if ((dfsr >> 11) & 1) {
        return 0;
    }
    return 1;
}

// get the pc of the fault.
//   - p13-34: use <wfar> (see 3-12) to get the fault pc 
// important:
//   - pay attention to the comment on 13-12 to see how to adjust!
uint32_t watchpt_fault_pc(void) {
    // return staff_watchpt_fault_pc();
    return cp14_wfar_get() - 8;

}

// get the data address that caused the fault.
// use <far> 3-68 to get the fault addr.
uint32_t watchpt_fault_addr(void) {
    // return staff_watchpt_fault_addr();
    return cp15_far_get();
}

// set a watch-point on <addr>: 
//  1. enable cp14 if not enabled.  
//     - MAKE SURE TO DO THIS FIRST.
//  2. set wcr0 (13-21), wvr0 (13-20)
//     - don't rmw -- just set it directly.
// Important: 
//  - make sure you handle subword accesses! 
int watchpt_on(uint32_t addr) {
    // trace("I am in watchpt_on, going to put in %d\n", addr);
    watch_addr = addr;
    // 1. enable cp14
    // get dscr value 
    uint32_t dscr = cp14_dscr_get();
    // set dscr  15 bits to 1, 14 to 0 
    cp14_dscr_set(bit_set(dscr, 15)); 
    dscr = cp14_dscr_get();
    cp14_dscr_set(bit_clr(dscr, 14)); 

    // 2. read WCR0 
    uint32_t wcr = cp14_wcr_get();
    // 3. clear WCR[0] and write it back to WCR to disable watchpoint 
    cp14_wcr_set(bit_clr(wcr, 0));
    
    // 4. Set the "watchpoint value register" (WVR) on 13-20 to 0.
    // uint32_t wvr = cp14_wvr_get();
    uint32_t val = (watch_addr >> 2) << 2;
    cp14_wvr_set(val);


    // 5. Set the "watchpoint control register" (WCR) on 13-21
    wcr = cp14_wcr_get();
    // disable linking
    cp14_wcr_set(bit_clr(wcr, 20));
    // watchpoint matches secure/nonsecure world
    wcr = cp14_wcr_get();
    cp14_wcr_set(bits_clr(wcr, 14, 15));
    // Byte address select for all accesses (0x0, 0x1, 0x2, 0x3).
    wcr = cp14_wcr_get();
    cp14_wcr_set(bits_set(wcr, 0, 8, 0b111111111));
    return addr;
}

// turn off watchpoint:
//   - check that <addr> is what we were watching.
//   - clear wcr0
int watchpt_off(uint32_t addr) {
    if(addr != watch_addr)
        panic("disabling invalid watchpoint %x, tracking %x\n", 
            addr, watch_addr);
    // return staff_watchpt_off(addr);
    // trace("addr = %x\n", addr);
    // uint32_t wvr = cp14_wvr_get(); 
    // trace(" wvr = %x\n", wvr);
    // assert(addr == (cp14_wvr_get() >> 2));

    uint32_t wcr = cp14_wcr_get();
    cp14_wcr_set(bit_clr(wcr, 0));
    return addr;
}
