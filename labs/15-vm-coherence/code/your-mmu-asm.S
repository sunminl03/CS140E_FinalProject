@ Must carefully read B2 of the armv6 manual.  You cannot rely on "well it
@ worked on my test case": mistakes cause stale data, which may or may not
@ show up in your test case (likely not) despite being wildly broken.
@
@ Note: Rd is allegedly a read-only parameter to these instructions, but
@ the manual says SBZ (should be zero).  I think (and the linux source seems
@ to think) that this means we have to explicitly set it to 0.  Not setting
@ Rd=0 is an easy mistake to make.  Perhaps is worth building in (Linux does
@ not, but) --- the cost of an extra mov is negligible given our other 
@ overheads.
@
@ Alot of people put these instructions in inline assembly, wrapped up by 
@ function calls (e.g., cs107e's MMU code).  This practice is dangerous. 
@ For example, if you are flushing caches, you cannot be sure that the 
@ function return, etc does not then reload them, esp w.r.t. the BTB.  I 
@ think the only safe approach is to put directly in assembly so you are 
@ guaranteed no branch, indirect jump, load/store happens in between.
@
@ A lot of MMU code out there appears pretty broken b/c they don't do enough
@ flushing operations and/or rely on gcc code generation to not do the above.

#include "rpi-asm.h"
#include "armv6-coprocessor-asm.h"


@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ many page numbers are in 
@    armv6-coprocessor-asm.h
@ and the other armv6-*.h files

// just read the cache type: use C code to figure out if it is unified
// b6-13, b6-14
MK_FN(get_cache_type)
    asm_not_implemented()
    bx lr

// void reset(void)
// clear the dcache, icache, tlbs
MK_FN(mmu_reset)
    @ b staff_mmu_reset
    // invalidate all caches
    CLEAN_INV_DCACHE(r0)
    INV_DCACHE(r0)
    INV_ICACHE(r0)
    INV_TLB(r0)
    DSB(r0) // pg. B2-22: execution of I cache, D cache, and TLB are only visible after DSB
    PREFETCH_FLUSH(r0)  // pg. B2-22: all caches are visible after Prefetch Flush

    bx lr

// void cp15_domain_ctrl_wr(uint32_t dom_reg);
// NOTE:
//   - definitely need to do prefetch flush 
//   - I don't *think* we need the full cp15 sync: flush_btb, 
//     dsb, prefetch flush.
MK_FN(cp15_domain_ctrl_wr)
    @ b staff_cp15_domain_ctrl_wr
    asm_not_implemented()

    @ FLUSH_BTB(r1)
    @ DSB(r1)
    @ PREFETCH_FLUSH(r1)
    bx lr

// ADDED
// cp15_ctrl_reg1_t cp15_ctrl_reg1_rd(void);
MK_FN(cp15_ctrl_reg1_rd) 
    // pg 3-47: Read Control Register configuration data. No need to flush since we are reading 
    mrc p15, 0, r0, c1, c0, 0
    bx lr

MK_FN(cp15_ctrl_reg1_wr)
    // pg 3-47: WriteControl Register configuration data
    mcr p15, 0, r0, c1, c0, 0
    // B2.7.6: prefetch flush after change in cp15 
    PREFETCH_FLUSH(r0)
    bx lr

// void cp15_set_procid_ttbr0(uint32_t proc_and_asid, fld_t *pt);
// sequence from b2-25
MK_FN(cp15_set_procid_ttbr0)
    @ b staff_cp15_set_procid_ttbr0
    @ asm_not_implemented()
    DSB(r0)
    // You will write the address of the page table to the page table register ttbr0, set both TTBR1 and TTBC to 0
    MOV r2, #0
    // 1. change ASID to 0
    ASID_SET(r2)
    // 2. BTBflush and prefetch flush after change in asid pg B2.7.5, B 2.6.3
    FLUSH_BTB(r0) 
    PREFETCH_FLUSH(r0)
    // 3. change translation table base register (TTBR)
    TTBR0_SET(r1)
    TTBR1_SET(r2)
    MCR p15, 0, r2, c2, c0, 2
    // pg B2.7.5: BTB Flush 
    FLUSH_BTB(r0) 
    // 4. prefetch flush
    PREFETCH_FLUSH(r0)
    // 5. change ASID to new value
    ASID_SET(r0)
    // pg B2.7.5: BTB Flush, Prefetch Flush 
    FLUSH_BTB(r0) 
    PREFETCH_FLUSH(r0)
    bx lr

// void mmu_disable_set_asm(cp15_ctrl_reg1_t c);
MK_FN(mmu_disable_set_asm)
    # b staff_mmu_disable_set_asm

    @ note: this piece of code is mapped into the same place in both
    @ MMU-enabled and MMU-disabled so BTB and Prefetch entries work identically
    @ to both MMU-enable and disable.   Without this requirement I believe is
    @ very hard to get right (perhaps impossible?) since we cannot flush
    @ the BTB and prefetch and switch MMU on/off atomically.  thus, would
    @ flush, but instructions could get fetched before we could disable,
    @ defeating the purpose.

    # asm_not_implemented()
    // 1. Clean, Invalidate Data Cache before Disabling. Or else, dirty data will be lost. 
    CLEAN_INV_DCACHE(r2)
    // 2. Disable Data Cache pg 6.4.2
    mrc p15, 0, r1, c1, c0, 0      @ read SCTLR into r0
    bic r1, r1, #(1 << 2)          @ clear bit 2 (C bit) → disable D-cache
    mcr p15, 0, r1, c1, c0, 0      @ write SCTLR back

    // 3. Make sure dcache is invalidated and disabled 
    DSB(r2)
    // 4. this data cache will be visible 
    PREFETCH_FLUSH(r2)
    // 5. write to cp15_ctrl_reg1_wr to DISABLE MMU  
    CONTROL_REG1_WR(r0)
    // 6. pg 2.7.2 whenever we enable/disable mmu, flush BTB
    FLUSH_BTB(r2)
    // 7. After BTB should be prefetch flush 
    PREFETCH_FLUSH(r2)
    bx lr

// Don't invalidate the TLBs or pinned-vm may not work.
// void mmu_enable_set_asm(cp15_ctrl_reg1_t c);
MK_FN(mmu_enable_set_asm)
    # b staff_mmu_enable_set_asm
    # asm_not_implemented()  
    // pg. 6-9
    // 1. Disable I Cache so that the CPU cannot fetch instruction from the I Cache while turning on the MMU. I Cache can have stale instructions
    mrc p15, 0, r1, c1, c0, 0      @ r1 = Control register value 
    bic r1, r1, #(1 << 12)           @ clear I bit
    mcr p15, 0, r1, c1, c0, 0      @ write back to control register 
    // 2. Invalidate I cache so that we fetch instructions again after mmu is on 
    INV_ICACHE(r0) 
    // 3. wait for I cache to be invalidated 
    DSB(r0) 
    // 4. Prefetch flush: pg B 2.7.2 completion of an Icache op is only going to be visible to the instr fetch after prefetch flush
    PREFETCH_FLUSH(r0)
    CONTROL_REG1_WR(r0) // pg. 3-47 Write Control Register configuration data. Enable MMU
    // pg B 2.7.2 whenever we enable/disable mmu, flush BTB
    FLUSH_BTB(r0) 
    // pg. B 2.7.5: Flush BTB only is visible after prefetch flush 
    PREFETCH_FLUSH(r0)
    // Reenable Icache
    mrc p15, 0, r1, c1, c0, 0      @ r1 = Control register value 
    orr r1, r1, #(1 << 12)           @ clear I bit
    mcr p15, 0, r1, c1, c0, 0      @ write back to control register
    PREFETCH_FLUSH(r0)
    bx lr

MK_FN(mmu_sync_pte_mods)
    @ b staff_mmu_sync_pte_mods

    @ asm_not_implemented()
    // B 2- 23
    // 1. Clean line [Page table entry], Invalidate D Cache = Flush D Cache
    // If we change the page table, the CPU might still have the old instructions cached
    // So we need to invalidate the cache so that we fetch new instructions
    INV_ICACHE(r0);
    // Page table might only be updated in cache. write back to memory so memory holds updated page table entries 
    CLEAN_INV_DCACHE(r0);
    // 2. DSB: ensure modifed page table entry is visible in memory
    DSB(r0)
    // 3. Invalidate TLB entry by MVA: Since we changed Page table, old entries might be in TLB. Need to invalidate
    INV_TLB(r0)
    // 4. Invalidate BTB: Branch predictor might hold old entries too 
    FLUSH_BTB(r0)
    // 5. DSB: make sure TLB is invalidated
    DSB(r0) 
    // 6. Prefetch Flush: Need it after FLUSH_BTB  pg. 2.7.5
    PREFETCH_FLUSH(r0)


    bx lr

