#ifndef __SLICING_H
#define __SLICING_H

#include "../util/util.h"

int get_seq_len();
int get_reduction_bits();
const int *get_base_sequence();
const uint64_t *get_xor_mask();
int get_address_slice(uint64_t address);

#endif //__SLICING_H