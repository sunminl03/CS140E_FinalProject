// circular-4peek-test.c
// Stronger than 1-element test but still short (runs fast under single-step).
// Uses: cq_push, cq_nelem, cq_peek_n, cq_ckpt, cq_pop_nonblock
// Tests:
//  - multiple element FIFO order (4 items)
//  - cq_peek_n correctness (non-destructive, tail restore)
//  - queue ends empty
//  - checksum agreement
//
// No locks.

#include "check-interleave.h"
#include "libc/circular.h"

static inline void compiler_mb(void) { asm volatile("" ::: "memory"); }

static cq_t cq;
static uint32_t expected_A, expected_B;

enum { K = 4 };
static const cqe_t vals[K] = { 7, 200, 13, 42 };

// A puts the known sequence [7,200,13,42] into the queue.
static void circq_A(checker_t *c) {
    // Keep this short: fixed number of operations, no loops.
    if(!cq_push(&cq, vals[0])) panic("push0 failed\n");
    compiler_mb();
    if(!cq_push(&cq, vals[1])) panic("push1 failed\n");
    compiler_mb();
    if(!cq_push(&cq, vals[2])) panic("push2 failed\n");
    compiler_mb();
    if(!cq_push(&cq, vals[3])) panic("push3 failed\n");
    compiler_mb();

    expected_A = (uint32_t)vals[0] + vals[1] + vals[2] + vals[3];
    compiler_mb();
}

// B (once there are 4 items) checks peek_n doesn’t consume and returns the right values, then pops everything and confirms FIFO + empty + checksum match.
static int circq_B(checker_t *c) {
    if(cq_nelem(&cq) < K)
        return 0;

    compiler_mb();

    // 1) peek_n should not consume
    cqe_t peeked[K];
    unsigned tail_before = cq_ckpt(&cq);
    if(!cq_peek_n(&cq, peeked, K))
        panic("peek_n failed even though nelem >= K\n");
    compiler_mb();

    if(cq_ckpt(&cq) != tail_before)
        panic("peek_n did not restore tail\n");

    for(unsigned i = 0; i < K; i++) {
        if(peeked[i] != vals[i])
            panic("peek mismatch: got=%d expected=%d at i=%d\n",
                  peeked[i], vals[i], i);
    }

    // 2) now pop for real
    expected_B = 0;
    for(unsigned i = 0; i < K; i++) {
        cqe_t e = 0;
        if(!cq_pop_nonblock(&cq, &e))
            panic("pop_nonblock failed (i=%d nelem=%d)\n", i, cq_nelem(&cq));
        if(e != vals[i])
            panic("FIFO mismatch: got=%d expected=%d at i=%d\n", e, vals[i], i);
        expected_B += e;
        compiler_mb();
    }

    if(cq_nelem(&cq) != 0)
        panic("queue not empty after popping K elements: nelem=%d\n", cq_nelem(&cq));

    return 1;
}

static void circq_init(checker_t *c) {
    cq_init(&cq, 0);
    expected_A = expected_B = 0;
    compiler_mb();
}

static int circq_check(checker_t *c) {
    compiler_mb();
    if(expected_A != expected_B) {
        printk("checksum mismatch: A=%u B=%u nelem=%u\n",
               expected_A, expected_B, cq_nelem(&cq));
        return 0;
    }
    if(cq_nelem(&cq) != 0) {
        printk("queue not empty at end: nelem=%u\n", cq_nelem(&cq));
        return 0;
    }
    return 1;
}

checker_t circq_mk_checker(void) {
    return (struct checker) {
        .state = 0,
        .A = circq_A,
        .B = circq_B,
        .init = circq_init,
        .check = circq_check,
    };
}

void notmain(void) {
    enable_cache();

    struct checker c = circq_mk_checker();
    if(!check(&c)) {
        assert(c.nerrors);
        panic("check failed unexpectedly, trials=%d errors=%d\n",
              c.ntrials, c.nerrors);
    } else {
        assert(!c.nerrors);
        exit_success("worked! ntrials=[%d]\n", c.ntrials);
    }
}
