// we modify the <0-nop-example.c> example, stripping out comments
// so its more succinct and wrapping up the diffent routines into
// a <single_step_fn> routine that will run a function with an 
// argument in single step mode.
#include "rpi.h"
#include "breakpoint.h"
#include "full-except.h"
#include "cpsr-util.h"
#include "single-step-syscalls.h"
#include "single-step.h"

#undef trace
#define trace(msg...) do { if(verbose_p) trace_nofn(msg); } while(0)

// complete example for how to run single stepping on a simple routine.
void notmain(void) {
    kmalloc_init_mb(1);

    // make a single stack at the same address for everyone.
    enum { stack_size = 64 * 1024 };
    void *stack = kmalloc(stack_size);

    // should match exactly.
    trace("******************<nop_1>******************************\n");
    trace("\tcheck the faulting pc's against <0-nop-example.list>:\n");
    trace("\t00008038 <nop_1>:\n");
    trace("expected:\t8038:   e320f000    nop {0}\n");
    trace("expected:\t803c:   e3a00002    mov r0, #2\n");
    trace("expected:\t8040:   ef000001    svc 0x00000001\n");

    regs_t r = {};
    void nop_1(void);
    single_step_fn("nop_1", nop_1, 0, 0, 0);

    trace("******************<nop_10>******************************\n");
    void nop_10(void);
    r = single_step_fn("nop_10", nop_10, 0, 0, 0);

    // this routine used a trampoline: check that the final pc
    // is correct.
    extern uint32_t exit_tramp_pc[]; // see <single-step-start.S>
    trace("expect: pc should be %x!\n", exit_tramp_pc);
    void *exit_pc = (void *)r.regs[15];
    if(exit_pc != exit_tramp_pc)
        panic("final pc should be = %x, is %x!\n", exit_tramp_pc, exit_pc);
    trace("success: exit pc=%x!\n", exit_pc);

    trace("******************<mov_ident>******************************\n");
    void mov_ident(void);
    single_step_fn("mov_ident", mov_ident, 0, 0, 0);

    // same as <nop_10> this routine used a trampoline: check that 
    // the final pc is correct.
    exit_pc = (void *)r.regs[15];
    if(exit_pc != exit_tramp_pc)
        panic("final pc should be = %x, is %x!\n", exit_tramp_pc, exit_pc);
    trace("success: exit pc=%x!\n", exit_pc);

    trace("******************<hello>******************************\n");
    trace("about to print hello world single-step, w/ yapping:\n");
    void hello_asm(void);
    n_inst = 0;
        single_step_fn("hello_asm", hello_asm, 0, stack, stack_size);
    trace("ran %d instructions\n", n_inst);

    trace("******************<hello>******************************\n");
    trace("about to print hello world single-step, no yapping:\n");
    verbose_p = 0;
    void hello_asm(void);
    n_inst = 0;
        single_step_fn("hello_asm", hello_asm, 0, stack, stack_size);
    trace("ran %d instructions\n", n_inst);
}
