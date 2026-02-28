#include "rpi.h"
#include "pinned-vm.h"
#include "full-except.h"
#include "memmap.h"
#include "asm-helpers.h"

#define MB(x) ((x)*1024*1024)

static volatile uint32_t saved_dacr;
static volatile uint32_t expected_pc;

// ---------- segments ----------
enum {
    SEG_CODE      = MB(0),
    SEG_HEAP      = MB(1),
    SEG_STACK     = STACK_ADDR - MB(1),
    SEG_INT_STACK = INT_STACK_ADDR - MB(1),
    SEG_BCM_0     = 0x20000000,
    SEG_BCM_1     = SEG_BCM_0 + MB(1),
    SEG_BCM_2     = SEG_BCM_0 + MB(2),
};

// ---------- CP15 fault regs ----------
cp_asm_get(cp15_dfsr, p15, 0, c5, c0, 0)
cp_asm_get(cp15_dfar, p15, 0, c6, c0, 0)

cp_asm_get(cp15_ifsr, p15, 0, c5, c0, 1)
cp_asm_get(cp15_ifar, p15, 0, c6, c0, 2)

// DFSR/IFSR: FS = (bit10<<4) | bits[3:0]
static inline uint32_t fs_from_fsr(uint32_t fsr) {
    return ((((fsr >> 10) & 1) << 4) | (fsr & 0xF));
}
static const char *fs_reason_str(uint32_t fs) {
    switch(fs) {
    case 0x9: return "Domain fault (section)";
    case 0xB: return "Domain fault (page)";
    case 0xD: return "Permission fault (section)";
    case 0xF: return "Permission fault (page)";
    case 0x5: return "Translation fault (section)";
    case 0x7: return "Translation fault (page)";
    default:  return "Other/unknown";
    }
}

// ---------- handlers ----------
static void data_abort_handler(regs_t *r) {
    uint32_t pc   = r->regs[15];
    uint32_t dfsr = cp15_dfsr_get();
    uint32_t dfar = cp15_dfar_get();
    uint32_t fs   = fs_from_fsr(dfsr);

    // Part 4 requirement: pc equals address of GET32/PUT32
    if(pc != expected_pc)
        panic("data abort: pc mismatch: expected=%x got=%x\n", expected_pc, pc);

    output("DATA ABORT: pc=%x dfar=%x dfsr=%x fs=%x (%s)\n",
           pc, dfar, dfsr, fs, fs_reason_str(fs));

    // Re-enable domain permissions and return so instruction retries.
    staff_domain_access_ctrl_set(saved_dacr);
}

static void prefetch_abort_handler(regs_t *r) {
    uint32_t pc   = r->regs[15];
    uint32_t ifsr = cp15_ifsr_get();
    uint32_t ifar = cp15_ifar_get();
    uint32_t fs   = fs_from_fsr(ifsr);

    if(pc != expected_pc)
        panic("prefetch abort: pc mismatch: expected=%x got=%x\n", expected_pc, pc);

    output("PREFETCH ABORT: pc=%x ifar=%x ifsr=%x fs=%x (%s)\n",
           pc, ifar, ifsr, fs, fs_reason_str(fs));

    staff_domain_access_ctrl_set(saved_dacr);
}

void notmain(void) {
    kmalloc_init_set_start((void*)SEG_HEAP, MB(1));
    assert((uint32_t)__prog_end__ < SEG_CODE + MB(1));
    assert((uint32_t)__code_start__ >= SEG_CODE);

    full_except_install(0);
    full_except_set_data_abort(data_abort_handler);
    // use the correct name in your codebase:
    full_except_set_prefetch(prefetch_abort_handler);

    void *null_pt = kmalloc_aligned(4096*4, 1<<14);
    assert((uint32_t)null_pt % (1<<14) == 0);

    enum { no_user = perm_rw_priv };
    enum { dom_kern = 1, dom_heap = 2 };

    pin_t dev  = pin_mk_global(dom_kern, no_user, MEM_device);
    pin_t kern = pin_mk_global(dom_kern, no_user, MEM_uncached);
    pin_t heap = pin_mk_global(dom_heap, no_user, MEM_uncached);

    staff_mmu_init();

    pin_mmu_sec(0, SEG_CODE,      SEG_CODE,      kern);
    pin_mmu_sec(1, SEG_HEAP,      SEG_HEAP,      heap);  // heap tagged with dom_heap
    pin_mmu_sec(2, SEG_STACK,     SEG_STACK,     kern);
    pin_mmu_sec(3, SEG_INT_STACK, SEG_INT_STACK, kern);

    pin_mmu_sec(4, SEG_BCM_0, SEG_BCM_0, dev);
    pin_mmu_sec(5, SEG_BCM_1, SEG_BCM_1, dev);
    pin_mmu_sec(6, SEG_BCM_2, SEG_BCM_2, dev);

    enum { ASID = 1, PID = 128 };
    staff_mmu_set_ctx(PID, ASID, null_pt);

    // enable kernel + heap domains
    uint32_t all_on =
        (DOM_client << (dom_kern*2)) |
        (DOM_client << (dom_heap*2));
    staff_domain_access_ctrl_set(all_on);
    saved_dacr = all_on;

    staff_mmu_enable();
    assert(staff_mmu_is_enabled());

    // build heap data BEFORE disabling heap domain
    volatile uint32_t *p = kmalloc(4);
    *p = 0x12345678;

    uint32_t heap_off =
        (DOM_client    << (dom_kern*2)) |
        (DOM_no_access << (dom_heap*2));

    // --------------------
    // 1) LOAD: fault in GET32
    // --------------------
    expected_pc = (uint32_t)GET32;
    staff_domain_access_ctrl_set(heap_off);
    uint32_t v = GET32((uint32_t)p);   // faults once, handler re-enables, retries, succeeds
    output("LOAD ok after handler: v=%x\n", v);
    assert(v == *p);

    // --------------------
    // 2) STORE: fault in PUT32
    // --------------------
    expected_pc = (uint32_t)PUT32;
    staff_domain_access_ctrl_set(heap_off);
    PUT32((uint32_t)p, 0xdeadbeef);    // faults once then succeeds
    output("STORE ok after handler: *p=%x\n", (uint32_t)*p);
    assert(*p == 0xdeadbeef);

    // --------------------
    // 3) JUMP: prefetch abort on heap fetch
    // --------------------
    uint32_t *code = kmalloc(4);
    *code = 0xE12FFF1E;                // bx lr

    expected_pc = (uint32_t)code;
    staff_domain_access_ctrl_set(heap_off);
    BRANCHTO((uint32_t)code);          // faults once then returns

    output("JUMP ok after handler\n");

    trace("ALL PART4 TESTS PASSED\n");
    clean_reboot();
}