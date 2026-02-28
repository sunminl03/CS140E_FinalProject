// put your code here.
//
#include "rpi.h"
#include "libc/bit-support.h"

// has useful enums and helpers.
#include "vector-base.h"
#include "pinned-vm.h"
#include "mmu.h"
#include "procmap.h"
#include "asm-helpers.h"

#define MB(x) ((x)*1024*1024)

cp_asm_fn(tlb_access, p15, 5, c15, c4, 2)
cp_asm_fn(lockdown_va2, p15, 5, c15, c5, 2)
cp_asm_fn(lockdown_attr2, p15, 5, c15, c7, 2)
cp_asm_fn(lockdown_pa2, p15, 5, c15, c6, 2 )
cp_asm_fn(translate, p15, 0, c7, c8, 0)
cp_asm_fn(get_pa, p15, 0, c7, c4, 0)
// generate the _get and _set methods.
// (see asm-helpers.h for the cp_asm macro 
// definition)
// arm1176.pdf: 3-149

static void *null_pt = 0;

// should we have a pinned version?
void domain_access_ctrl_set(uint32_t d) {
    staff_domain_access_ctrl_set(d);
}

// fill this in based on the <1-test-basic-tutorial.c>
// NOTE: 
//    you'll need to allocate an invalid page table
void pin_mmu_init(uint32_t domain_reg) {
    // staff_pin_mmu_init(domain_reg);

    staff_mmu_init();
    null_pt = kmalloc_aligned(4096*4, 1<<14);
    assert((uint32_t)null_pt % (1<<14) == 0);
    domain_access_ctrl_set(domain_reg);
    return;
}

// do a manual translation in tlb:
//   1. store result in <result>
//   2. return 1 if entry exists, 0 otherwise.
//
// NOTE: mmu must be on (confusing).
int tlb_contains_va(uint32_t *result, uint32_t va) {
    assert(mmu_is_enabled());

    // 3-79
    assert(bits_get(va, 0,2) == 0);
    // write to va reg 
    translate_set(va);
    // Read back out
    uint32_t pa_reg_val = get_pa_get();
    *result = (bits_get(pa_reg_val, 10, 31) << 10) | bits_get(va, 0, 9);

    return !bits_get(pa_reg_val, 0, 0);


    // return staff_tlb_contains_va(result, va);

}

// map <va>-><pa> at TLB index <idx> with attributes <e>
void pin_mmu_sec(unsigned idx,  
                uint32_t va, 
                uint32_t pa,
                pin_t e) {


    demand(idx < 8, lockdown index too large);
    // lower 20 bits should be 0.
    demand(bits_get(va, 0, 19) == 0, only handling 1MB sections);
    demand(bits_get(pa, 0, 19) == 0, only handling 1MB sections);

    debug("about to map %x->%x\n", va,pa);


    // // delete this and do add your code below.
    // staff_pin_mmu_sec(idx, va, pa, e);
    // return;

    // these will hold the values you assign for the tlb entries.
    uint32_t x, va_ent, pa_ent, attr;

    // Load TLB Lockdown entry
    // x = tlb_access_get();
    // x = bits_set(x, 0, 2, idx);
    tlb_access_set(idx);

    // Write TLB Lockdown VA
    va_ent = va; 
    va_ent = bits_set(va_ent, 9, 9, e.G);
    if (!e.G) { // if entry is app-specific, set the ASID
        va_ent = bits_set(va_ent, 0, 7, e.asid);
    }
    // va_ent = bits_set(va_ent,0, 7, e.asid);
    lockdown_va2_set(va_ent);

    // Write TLB Lockdown Attrs
    attr = 0;
    attr = bits_set(attr, 1, 5, e.mem_attr);
    attr = bits_set(attr, 7, 10, e.dom);
    lockdown_attr2_set(attr);

    // Write TLB Lockdown PA
    pa_ent = pa;
    pa_ent = bits_set(pa_ent, 6, 7, e.pagesize);
    pa_ent = bits_set(pa_ent, 1, 3, e.AP_perm);
    pa_ent = bit_set(pa_ent, 0);
    lockdown_pa2_set(pa_ent);
    // panic("im here\n");
    return;
    

#if 0
    if((x = lockdown_va2_get()) != va_ent)
        panic("lockdown va: expected %x, have %x\n", va_ent,x);
    if((x = lockdown_pa2_get()) != pa_ent)
        panic("lockdown pa: expected %x, have %x\n", pa_ent,x);
    if((x = lockdown_attr2_get()) != attr)
        panic("lockdown attr: expected %x, have %x\n", attr,x);
#endif
}

// check that <va> is pinned.  
int pin_exists(uint32_t va, int verbose_p) {
    if(!mmu_is_enabled())
        panic("XXX: i think we can only check existence w/ mmu enabled\n");

    uint32_t r;
    if(tlb_contains_va(&r, va)) {
        // panic("r = %x, va = %x\n", r, va);
        assert(va == r);
        return 1;
    } else {
        if(verbose_p) 
            output("TLB should have %x: returned %x [reason=%b]\n", 
                va, r, bits_get(r,1,6));
        return 0;
    }
}

// look in test <1-test-basic.c> to see what to do.
// need to set the <asid> before turning VM on and 
// to switch processes.
void pin_set_context(uint32_t asid) {
    // put these back
    demand(asid > 0 && asid < 64, invalid asid);
    demand(null_pt, must setup null_pt --- look at tests);

    // staff_pin_set_context(asid);

    // staff_domain_access_ctrl_set(DOM_client << dom_kern*2); 


    enum { PID = 128 };
    staff_mmu_set_ctx(PID, asid, null_pt);
}

void pin_clear(unsigned idx)  {
    // staff_pin_clear(idx);
    // we will be accessing tlb idx 
    tlb_access_set(idx);
    // set all other 3 regs to 0 
    lockdown_va2_set(0);
    lockdown_attr2_set(0);
    lockdown_pa2_set(0);
    uint32_t x = 0;
#if 1
    if((x = lockdown_va2_get()) != 0)
        panic("lockdown va: expected %x, have %x\n", 0,x);
    if((x = lockdown_pa2_get()) != 0)
        panic("lockdown pa: expected %x, have %x\n", 0,x);
    if((x = lockdown_attr2_get()) != 0)
        panic("lockdown attr: expected %x, have %x\n", 0,x);
#endif
    
    
}

void lockdown_print_entry(unsigned idx) {
    // panic("make sure to use <trace_nofn> not <trace>\n");
    trace_nofn("   idx=%d\n", idx);
    tlb_access_set(idx);
    uint32_t va_ent = lockdown_va2_get();
    uint32_t pa_ent = lockdown_pa2_get();
    uint32_t attr = lockdown_attr2_get();
    unsigned v = bit_get(pa_ent, 0);

    if(!v) {
        trace_nofn("     [invalid entry %d]\n", idx);
        return;
    }
    uint32_t va = bits_clr(va_ent, 0, 11);
    va = va >> 12;
    uint32_t G = bit_get(va_ent, 9);
    uint32_t asid = bits_get(va_ent, 0, 7);
    trace_nofn("     va_ent=%x: va=%x|G=%d|ASID=%d\n",
            va_ent, va, G, asid);
    
    uint32_t pa = bits_clr(pa_ent, 0, 11);
    pa = pa >> 12;
    uint32_t nsa = bit_get(pa_ent, 9);
    uint32_t nstid = bit_get(pa_ent, 8);
    uint32_t size = bits_get(pa_ent, 6, 7);
    uint32_t apx = bits_get(pa_ent, 1, 3);

    trace_nofn("     pa_ent=%x: pa=%x|nsa=%d|nstid=%d|size=%b|apx=%b|v=%d\n",
            pa_ent, pa, nsa,nstid,size, apx,v);
    
    uint32_t dom = bits_get(attr, 7, 10);
    uint32_t xn = bit_get(attr, 6);
    uint32_t tex = bits_get(attr, 3, 5);
    uint32_t C = bit_get(attr, 2);
    uint32_t B = bit_get(attr, 1);
    trace_nofn("     attr=%x: dom=%d|xn=%d|tex=%b|C=%d|B=%d\n",
                attr, dom,xn,tex,C,B);

}

void lockdown_print_entries(const char *msg) {

#if 1
    trace_nofn("-----  <%s> ----- \n", msg);
    trace_nofn("  pinned TLB lockdown entries:\n");

    for(int i = 0; i < 8; i++)
        lockdown_print_entry(i);
    trace_nofn("----- ---------------------------------- \n");
#endif
}

