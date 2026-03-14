#include "rpi.h"
#include "wrap32.h"

/*
expected output:
    WRAP32 TEST START
    wrap32 basic: 1
    unwrap first byte: 0x1
    unwrap after wrap: 0x100000001
    unwrap before third wrap: 0x2fffffffe
    unwrap nonzero isn: 0x0
    unwrap big nonzero isn: 0xffffffff
    unwrap near half: 0x7fffffff
    unwrap checkpoint 1: 0xffffffff
    WRAP32 TEST DONE
*/

void notmain(void) {
    printk("WRAP32 TEST START\n");

    uint32_t wrapped = wrap32(1, 0);
    printk("wrap32 basic: %d\n", wrapped);
    assert(wrapped == 1);

    uint64_t u1 = unwrap32(1, 0, 0);
    printk("unwrap first byte: %llx\n", u1);
    assert(u1 == 1ULL);

    uint64_t u2 = unwrap32(1, 0, 0xFFFFFFFFULL);
    printk("unwrap after wrap: %llx\n", u2);
    assert(u2 == (1ULL << 32) + 1);

    uint64_t u3 = unwrap32(0xFFFFFFFEu, 0, 3ULL * (1ULL << 32));
    printk("unwrap before third wrap: %llx\n", u3);
    assert(u3 == 3ULL * (1ULL << 32) - 2);

    uint64_t u4 = unwrap32(16, 16, 0);
    printk("unwrap nonzero isn: %llx\n", u4);
    assert(u4 == 0ULL);

    uint64_t u5 = unwrap32(15, 16, 0);
    printk("unwrap big nonzero isn: %llx\n", u5);
    assert(u5 == 0xFFFFFFFFULL);

    uint64_t u6 = unwrap32(0xFFFFFFFFu, 1UL << 31, 0);
    printk("unwrap near half: %llx\n", u6);
    assert(u6 == (0xFFFFFFFFULL >> 1));

    uint64_t u7 = unwrap32(0, 1, 1);
    printk("unwrap checkpoint 1: %llx\n", u7);
    assert(u7 == 0xFFFFFFFFULL);

    printk("WRAP32 TEST DONE\n");
    clean_reboot();
}
