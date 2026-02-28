// circular-barrier-test.c
// Lockless circular-queue interleaving test w/ *compiler* memory barriers.
// Uses: cq_push, cq_nelem, cq_pop_nonblock
// Has a Busy Wait
// Updated to match your circular.h:
//  - cqe_t is unsigned char by default (8-bit).  So we only push 8-bit values.
//  - the queue itself already uses gcc_mb(); we still add barriers in the TEST
//    to keep the compiler from reordering our bookkeeping / checks across calls.
//
// This test checks:
//   1) FIFO order for byte values 0..N-1
//   2) queue ends empty
//   3) producer/consumer checksum agreement
//
// NOTE: we use cq_pop_nonblock (not cq_pop) so B won't deadlock if interrupts
// are disabled (and in interleaving, B runs in privileged mode).

#include "check-interleave.h"
#include "libc/circular.h"

enum { N = 8 };   // larger than 8, still < 256 so values are unique in 8-bit

// ------------------------------
// Compiler-only memory barrier (like gcc_mb()).
static inline void compiler_mb(void) {
    asm volatile("" ::: "memory");
}
static inline void compiler_wmb(void) { compiler_mb(); }
static inline void compiler_rmb(void) { compiler_mb(); }

// ------------------------------
static uint32_t expected_A, expected_B;
static cq_t cq;

static void circq_A(checker_t *c) {
    for (unsigned i = 0; i < N; i++) {
        cqe_t v = (cqe_t)i;     // fits in unsigned char

        // cq_push returns 0 if full.  Since B might not have run yet,
        // we *spin* until there is space (no locks).
        while (!cq_push(&cq, v)) {
            compiler_mb();
        }

        // Don't let our bookkeeping float above the publish.
        compiler_wmb();
        expected_A += v;
        compiler_mb();
    }
}

// B returns 0 if it can't make progress yet.
static int circq_B(checker_t *c) {
    if (cq_nelem(&cq) < N)
        return 0;

    // After seeing "enough elements", don't allow later loads to float up.
    compiler_rmb();

    for (unsigned i = 0; i < N; i++) {
        cqe_t e = 0;

        // Nonblocking pop: should succeed because we checked nelem >= N.
        if (!cq_pop_nonblock(&cq, &e))
            panic("pop_nonblock failed unexpectedly (i=%d, nelem=%d)\n",
                  i, cq_nelem(&cq));

        compiler_mb();

        cqe_t expected = (cqe_t)i;
        if (e != expected)
            panic("FIFO/corruption: got=%d expected=%d at i=%d (nelem=%d)\n",
                  e, expected, i, cq_nelem(&cq));

        expected_B += e;
        compiler_mb();
    }

    compiler_rmb();
    assert(cq_nelem(&cq) == 0);
    return 1;
}

static void circq_init(checker_t *c) {
    cq_init(&cq, 0);
    expected_A = 0;
    expected_B = 0;
    compiler_mb();
}

static int circq_check(checker_t *c) {
    compiler_mb();
    if (expected_A != expected_B) {
        printk("checksum mismatch: A=%u B=%u nelem=%u\n",
               expected_A, expected_B, cq_nelem(&cq));
        return 0;
    }
    if (cq_nelem(&cq) != 0) {
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
    if (!check(&c)) {
        assert(c.nerrors);
        panic("check failed unexpectedly, trials=%d, errors=%d!!\n",
              c.ntrials, c.nerrors);
    } else {
        assert(!c.nerrors);
        exit_success("worked! ntrials=[%d]\n", c.ntrials);
    }
}
