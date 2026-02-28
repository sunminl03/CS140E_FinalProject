// engler: starter code for simple A() B() interleaving checker.
// shows 
//   1. how to call the routines.
//   2. how to run code in single step mode.
//
// you'll have to rewrite some of the code to do the actual switching
// of A->B.
#include "check-interleave.h"
#include "full-except.h"
#include "pi-sys-lock.h"

// used to communicate with the breakpoint handler.
static volatile checker_t *checker = 0;

int brk_verbose_p = 0;

static void A_terminated(uint32_t ret);

// static inline int sys_lock_try(volatile int *l) {
//         // in rpi-inline-asm.h
//         uint32_t cpsr = cpsr_get();
        
//         // libpi/include/cpsr-util.h
//         if(mode_get(cpsr) == USER_MODE)
//             return syscall_invoke_asm(SYS_TRYLOCK, l);
//         else
//             // todo("just call the trylock directly\n");
//             return *l;

// }



// invoked from user level: 
//   syscall(sysnum, arg0, arg1, ...)
//
//   - sysnum number is in r->reg[0];
//   - arg0 of the call is in r->reg[1];
//   - arg1 of the call is in r->reg[2];
//   - arg2 of the call is in r->reg[3];
// we don't handle more arguments.
static int syscall_handler_full(regs_t *r) {
    assert(mode_get(cpsr_get()) == SUPER_MODE);

    // we store the first syscall argument in r1
    uint32_t arg0 = r->regs[1];
    uint32_t sys_num = r->regs[0];

    // pc = address of instruction right after 
    // the system call.
    uint32_t pc = r->regs[15];      
    int result;
    int tmp;
    volatile int *l = (volatile int*)arg0;

    switch(sys_num) {
    case SYS_RESUME:
            // used to do a context switch from user->privileged.
            switchto((void*)arg0);
            panic("not reached\n");
    case SYS_TRYLOCK:
        if (*l == 0) {
            *l = 1;
            return 1;
        } else {
            return 0;
        }

    case SYS_TEST:
        printk("running empty syscall with arg=%d\n", arg0);
        return SYS_TEST;
    default:
        panic("illegal system call = %d, pc=%x, arg0=%d!\n", sys_num, pc, arg0);
    }
}

// called on each single step exception: 
// if we have run switch_on_inst_n instructions then:
//  1. run B().
//     - if B() succeeds, then run the rest of A() to completion.  
//     - if B() returns 0, it couldn't complete w current state:
//       resume A() and switch on next instruction.

static void single_step_handler_full(regs_t *r) {
    // assume we only get breakpoint faults.
    if(!brkpt_fault_p())
        panic("impossible: should get no other faults\n");

    // r0 is in r->regs[0], r1 is in r->regs[1], ...
    uint32_t pc = r->regs[15];
    uint32_t n = ++checker->inst_count;


    // TODO: you'll have to add code to do the switching here.
    // 

    // if we are interleaving, we need to switch to B after n instructions
    if (checker->interleaving_p) {
        // trace everytime we are raising exception
        output("single-step handler: inst=%d: A:pc=%x\n", n,pc);
        // if we ran n number of A instructions, run B 
        if (n == checker->switch_on_inst_n) {
            if (checker->B((void*)checker)) { 
                // at this point, B is done running 
                // 
                checker->switch_addr = pc; 
                checker->switched_p = 1;
                checker->nswitches += 1;
                brkpt_mismatch_stop();
                switchto(r);
            } else {
                checker->switch_on_inst_n++;
                checker->skips++;
            }

        }
        brkpt_mismatch_set(pc);
        switchto(r);
        
    } else {
        output("no interleaved: single-step handler: inst=%d: A:pc=%x\n", n,pc);
        if (pc == (uint32_t)A_terminated) {
            brkpt_mismatch_stop();
        
        }
        brkpt_mismatch_set(pc);
        switchto(r);
    }
    // go back to next instruction in A 
    // switchto(r);


    // switch to back.
    
}

// registers saved when we started: recall similar in <rpi-thread.c>
static regs_t start_regs;

// this is called when A() returns: assumes you are at user level.
// switch back to <start_regs>
static void A_terminated(uint32_t ret) {
    uint32_t cpsr = mode_get(cpsr_get());
    if(cpsr != USER_MODE)
        panic("should be at USER, at <%s> mode\n", mode_str(cpsr));

    // put whatever A() returned into r0 position.
    start_regs.regs[0] = ret;
    sys_switchto(&start_regs);
}

// run routine <c->A()> at user level by making a stack,
// switching into it 
static uint32_t run_A_at_userlevel(checker_t *c) {
    // 1. get current cpsr and change mode to USER_MODE.
    // this will preserve any interrupt flags etc.
    uint32_t cpsr_cur = cpsr_get();
    assert(mode_get(cpsr_cur) == SUPER_MODE);
    uint32_t cpsr_A = mode_set(cpsr_cur, USER_MODE);

    // 2. setup the registers.

    // allocate stack on our main stack.  grows down.
    enum { N=8192 };
    uint64_t stack[N];

    // initialize the thread block for <A>.
    // see <switchto.h>
    regs_t r = { 
        // start running A.
        .regs[REGS_PC] = (uint32_t)c->A,
        // the first argument to A()
        .regs[REGS_R0] = (uint32_t)c,

        // stack pointer register
        .regs[REGS_SP] = (uint32_t)&stack[N],
        // the cpsr to use when running A()
        .regs[REGS_CPSR] = cpsr_A,

        // hack: setup registers so that when A() is done 
        // and returns will jump to <A_terminated> by 
        // setting the LR register to <A_terminated>.
        .regs[REGS_LR] = (uint32_t)A_terminated,
    };

    // 3. turn on mismatching
    brkpt_mismatch_start();

    // 4. context switch to <r> and save current execution
    // state in <start_regs> [similar to <rpi-thread.c>
    // when we started the threads package.
    brkpt_mismatch_set(r.regs[REGS_PC]);  
    switchto_cswitch(&start_regs, &r);

    // 5. At this point A() finished execution.   We got 
    // here from A_terminated():switchto(&start_regs)

    // 6. turn off mismatch (single step)
    brkpt_mismatch_stop();

    // 7. return back to the checking loop.
    return start_regs.regs[0];
}

// <c> has pointers to four routines:
//  1. A(), B(): the two routines to check.
//  2. init(): initialize A() B() state.
//  3. check(): called after A()B() and returns 1 if checks out.
int check(checker_t *c) {
    // install exception handlers for 
    // (1) system calls
    // (2) prefetch abort (single stepping exception is a type
    //     of prefetch fault)
    // 
    // install is idempotent if already there.
    full_except_install(0);
    full_except_set_syscall(syscall_handler_full);
    full_except_set_prefetch(single_step_handler_full);
    // initialize checks
    c->nswitches = 0;

    // show how to the interface works by testing
    // A(),B() sequentially multiple times.
    // 
    // if the code is non-deterministic, or the init / check
    // routines are busted, this can detect it.
    //
    // if AB commuted, we could also check BA but this won't be 
    // true in general.
    for(int i = 0; i < 10; i++) {
        // 1.  initialize the state.
        c->init(c);     
        // 2. run A()
        c->A(c);
        // 3. run B(): currently require that can't fail given that 
        //    A() ran.
        if(!c->B(c)) 
            panic("B should not fail\n");
        // 4. check that the state passes.
        if(!c->check(c))
            panic("check failed sequentially: code is broken\n");
    }

    // shows how to run code with single stepping: do the same sequential
    // checking but run A() in single step mode: 
    // should still pass (obviously)
    checker = c;
    for(int i = 0; i < 10; i++) {
        c->init(c);
        run_A_at_userlevel(c);
        if(!c->B(c))
            panic("B should not fail\n");
        if(!c->check(c))
            panic("check failed sequentially: code is broken\n");
    }

    //******************************************************************
    // this is what you build: check that A(),B() code works
    // with one context switch for each possible instruction in
    // A()
    //
    // for(int i = 0; ; i++) {
    //      int switched_p = 0;
    //      c->init()
    //      run c->A() in single step for i instructions.
    //
    //      single step handler: 
    //          if this is the ith instruction
    //              call B().  
    //              if B() returned 1 (B() could run)
    //                  switched_p = 1;
    //                  finish A() without single stepping
    //              else
    //                  run A() one more another instruction and try B()
    // 
    //      checker:  when done running A(),B():
    //          c->check()
    //          if(!switched_p)
    //              break;
    //  }
    // 
    //  return 0 if there were errors.
    // todo("implement true interleaving!\n");
    c->nerrors = 0;
    c->interleaving_p = 1;
    for (int i = 1; ; i++) {
        c->switched_p = 0;
        c->inst_count = 0;
        c->init(c);
        c->switch_on_inst_n = i; 
        c->ntrials++;
        run_A_at_userlevel(c);
        // if A ran till the end before switching to B, break. 
        if (!c->switched_p) {
            break;
        }
        // check status of c to see if we failed 
        if (!c->check(c)) {
            printk("ERROR: check failed when switched on address  instructions\n");
            c->nerrors++;
        }
    }

    return c->nerrors == 0;
}
