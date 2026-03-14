#ifndef __WRAP32_H__
#define __WRAP32_H__

#include "rpi.h"

uint32_t wrap32(uint64_t n, uint32_t isn);
uint64_t unwrap32(uint32_t seqno, uint32_t isn, uint64_t checkpoint);

#endif
