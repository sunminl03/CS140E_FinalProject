#include "wrap32.h"

uint32_t wrap32(uint64_t n, uint32_t isn) {
    return isn + (uint32_t)n;
}

uint64_t unwrap32(uint32_t seqno, uint32_t isn, uint64_t checkpoint) {
    uint32_t offset = seqno - isn;
    uint64_t checkpoint_factor = checkpoint / (1ULL << 32);
    uint32_t checkpoint_offset = (uint32_t)checkpoint;

    uint64_t upper_factor = offset > checkpoint_offset ? checkpoint_factor : checkpoint_factor + 1;
    uint64_t lower_factor = upper_factor == 0 ? 0 : upper_factor - 1;

    uint64_t upper_value = upper_factor * (1ULL << 32) + offset;
    uint64_t lower_value = lower_factor * (1ULL << 32) + offset;

    return upper_value - checkpoint < checkpoint - lower_value ? upper_value : lower_value;
}
