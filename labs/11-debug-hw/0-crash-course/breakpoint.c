#include "rpi.h"
#include "rpi-interrupts.h"
#include "asm-helpers.h"
#include "bit-support.h"
#include "breakpoint.h"

// DSCR pg. 13-7 ,
cp_asm_fn(cp14_dscr2, p14, 0, c0, c1, 0) 

// BCR pg 13-17
cp_asm_fn(cp14_bcr1, p14, 0, c0, c1, 5) 

// BVR pg 13-17
cp_asm_fn(cp14_bvr1, p14, 0, c0, c1, 4) 

// BCR pg 13-17
cp_asm_fn(cp14_bcr0, p14, 0, c0, c0, 5) 

// BVR pg 13-17
cp_asm_fn(cp14_bvr0, p14, 0, c0, c0, 4) 

// IFSR 3-66
cp_asm_fn(cp15_ifsr, p15, 0, c5, c0, 1)


// make sure that cp14 (DSCR) is enabled (p13-7)
void brkpt_match_init(void) {
    // staff_brkpt_match_init();
    uint32_t dscr = cp14_dscr2_get();
    // set dscr  14, 15 bits to 1 
    cp14_dscr2_set(bit_set(dscr, 15)); 
    dscr = cp14_dscr2_get();
    cp14_dscr2_set(bit_clr(dscr, 14)); 

}

// set match on <addr>
//
// for simplicity: for matching we use breakpoint 1 
// (bcr1 p13-17 and bvr1 p13-16) so dont't conflict 
// with single-stepping.   
void brkpt_match_set(uint32_t addr) {
    // disable bcr while setting bvr 
    brkpt_match_init();
    uint32_t bcr = cp14_bcr1_get();
    cp14_bcr1_set(bit_clr(bcr, 0));

    // set breakpoint 
    uint32_t val = (addr >> 2) << 2;
    cp14_bvr1_set(val);

    // set bcr 
    bcr = cp14_bcr1_get();
    bcr = bits_clr(bcr, 21, 22);
    
    bcr = bit_clr(bcr,20);
    bcr = bits_clr(bcr, 14, 15);
    bcr = bits_set(bcr, 5, 8, 0b1111);
    bcr = bits_set(bcr, 0, 2, 0b111);
    cp14_bcr1_set(bcr);

    // staff_brkpt_match_set(addr);
}

// turn off match faults (clear bcr1)
void brkpt_match_stop(void) {
    // staff_brkpt_match_stop();
    uint32_t bcr = cp14_bcr1_get();
    cp14_bcr1_set(bit_clr(bcr, 0));

}

// return the match addr (bvr1)
uint32_t brkpt_match_get(void) {
    // return staff_brkpt_match_get();
    uint32_t bcr = cp14_bcr1_get();
    if (!(bcr & 1)) {
        return 0;
    }
    uint32_t bvr = cp14_bvr1_get();
    return bvr;

}


// set mismatch on <addr>
//
// for simplicity: for matching we use breakpoint 0. 
// so set bcr0 and bvr0.
void brkpt_mismatch_set(uint32_t addr) {

    // staff_brkpt_mismatch_set(addr);

    uint32_t bcr = cp14_bcr0_get();
    cp14_bcr0_set(bit_clr(bcr, 0));

    // set breakpoint 
    uint32_t val = (addr >> 2) << 2;
    cp14_bvr0_set(val);

    // set bcr 
    bcr = cp14_bcr0_get();
    bcr = bits_set(bcr, 21, 22, 0b10);
    
    bcr = bit_clr(bcr,20);
    bcr = bits_clr(bcr, 14, 15);
    bcr = bits_set(bcr, 5, 8, 0b1111);
    bcr = bits_set(bcr, 0, 2, 0b111);
    cp14_bcr0_set(bcr);

    // staff_brkpt_match_set(addr);
}

// this will mismatch on the first instruction at user level.
void brkpt_mismatch_start(void) {
    // 1. check DSCR: if not enabled, enable it.
    // 2. set brkpt_mismatch_set(0)
    uint32_t dscr = cp14_dscr2_get();
    if (!(dscr >> 15 & 1)) {
        cp14_dscr2_set(bit_set(dscr, 15));
    }
    brkpt_mismatch_set(0);
    // staff_brkpt_mismatch_start();
}

// turn off mismatching: clear bcr0
void brkpt_mismatch_stop(void) {

    // staff_brkpt_mismatch_stop();
    uint32_t bcr = cp14_bcr0_get();
    cp14_bcr0_set(bit_clr(bcr, 0));
}

// was this a breakpoint fault? (either mismatch or match)
// check IFSR bits (p 3-66) to see it was a debug fault.
// check DSCR bits (13-11) to see if it was a breakpoint
int brkpt_fault_p(void) {
    // return staff_brkpt_fault_p();
    uint32_t ifsr = cp15_ifsr_get();
    if ((ifsr & 2)) {
        uint32_t dscr = cp14_dscr2_get();
        if ((dscr >> 2)&1) {
            return 1;
        }
    }
    return 0;
}
